#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "esp_event.h"

#include "esp_system.h"
#include "cJSON.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "common.h"


#define JSON_URL_LEN           512
#define REQ_JSON_LEN           4096
#define MAX_HTTP_OUTPUT_BUFFER 4096
#define BASE64_BUFFER_LEN      256

static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static void base64_encode(const char *input, char *output, int input_len) {
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (input_len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                *output++ = base64_table[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int k = i; k < 3; k++)
            char_array_3[k] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (int k = 0; k < i + 1; k++)
            *output++ = base64_table[char_array_4[k]];

        while (i++ < 3)
            *output++ = '=';
    }
    *output = '\0';
}

static int _parse_agora_agent_response(const char *resp_data, const int data_len)
{
    if (data_len == 0) {
        return -1;
    }

    cJSON *root_json = cJSON_Parse(resp_data);
    if (root_json == NULL) {
        printf("JSON parse error\n");
        return -1;
    }

    cJSON *agent_id = cJSON_GetObjectItemCaseSensitive(root_json, "agent_id");
    if (cJSON_IsString(agent_id) && (agent_id->valuestring != NULL)) {
        strncpy(g_app.agent_id, agent_id->valuestring, sizeof(g_app.agent_id) - 1);
        g_app.agent_id[sizeof(g_app.agent_id) - 1] = '\0';
        printf("Agent ID: %s\n", g_app.agent_id);
    }

    cJSON *status = cJSON_GetObjectItemCaseSensitive(root_json, "status");
    if (cJSON_IsString(status) && (status->valuestring != NULL)) {
        printf("Status: %s\n", status->valuestring);
    }

    cJSON_Delete(root_json);
    return 0;
}

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;
    static int output_len;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_DATA:
            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = 0;
                if (evt->user_data) {
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            printf("Failed to allocate memory for output buffer\n");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (output_buffer != NULL && output_len > 0) {
                _parse_agora_agent_response(output_buffer, output_len);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}


// Build Agora AI Agent join request JSON
static char *_build_agora_agent_join_json(void)
{
    char *json_ptr = NULL;

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return NULL;
    }

    // Add name (unique identifier)
    cJSON_AddStringToObject(root, "name", AI_AGENT_NAME);

    // Create properties object
    cJSON *properties = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "properties", properties);

    // Add channel info
    cJSON_AddStringToObject(properties, "channel", AI_AGENT_CHANNEL_NAME);
    cJSON_AddStringToObject(properties, "token", AI_AGENT_TOKEN);
    cJSON_AddStringToObject(properties, "agent_rtc_uid", "1001");

    // Add remote_rtc_uids array
    cJSON *remote_uids = cJSON_CreateArray();
    char uid_str[16];
    snprintf(uid_str, sizeof(uid_str), "%d", AI_AGENT_USER_ID);
    cJSON_AddItemToArray(remote_uids, cJSON_CreateString(uid_str));
    cJSON_AddItemToObject(properties, "remote_rtc_uids", remote_uids);

    // Add parameters
    cJSON *parameters = cJSON_CreateObject();
    cJSON_AddStringToObject(parameters, "output_audio_codec", "PCMU");
    cJSON_AddItemToObject(properties, "parameters", parameters);

    // Add idle_timeout
    cJSON_AddNumberToObject(properties, "idle_timeout", 120);

    // Add advanced_features
    cJSON *advanced_features = cJSON_CreateObject();
    cJSON_AddBoolToObject(advanced_features, "enable_aivad", true);
    cJSON_AddItemToObject(properties, "advanced_features", advanced_features);

    // Create LLM object
    cJSON *llm = cJSON_CreateObject();
    cJSON_AddStringToObject(llm, "url", LLM_API_URL);
    cJSON_AddStringToObject(llm, "api_key", LLM_API_KEY);

    // Add system_messages array
    cJSON *system_messages = cJSON_CreateArray();
    cJSON *system_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(system_msg, "role", "system");
    cJSON_AddStringToObject(system_msg, "content", LLM_SYSTEM_MESSAGE);
    cJSON_AddItemToArray(system_messages, system_msg);
    cJSON_AddItemToObject(llm, "system_messages", system_messages);

    cJSON_AddNumberToObject(llm, "max_history", 32);
    cJSON_AddStringToObject(llm, "greeting_message", LLM_GREETING);
    cJSON_AddStringToObject(llm, "failure_message", "Please hold on a second.");

    // Add LLM params
    cJSON *llm_params = cJSON_CreateObject();
    cJSON_AddStringToObject(llm_params, "model", LLM_MODEL);
    cJSON_AddItemToObject(llm, "params", llm_params);

    cJSON_AddItemToObject(properties, "llm", llm);

    // Create TTS object
    cJSON *tts = cJSON_CreateObject();
    cJSON_AddStringToObject(tts, "vendor", TTS_VENDOR);

    cJSON *tts_params = cJSON_CreateObject();
    cJSON_AddStringToObject(tts_params, "key", TTS_API_KEY);
    cJSON_AddStringToObject(tts_params, "region", TTS_REGION);
    cJSON_AddStringToObject(tts_params, "voice_name", TTS_VOICE_NAME);
    cJSON_AddItemToObject(tts, "params", tts_params);

    cJSON_AddItemToObject(properties, "tts", tts);

    // Create ASR object
    cJSON *asr = cJSON_CreateObject();
    cJSON_AddStringToObject(asr, "language", ASR_LANGUAGE);
    cJSON_AddItemToObject(properties, "asr", asr);

    json_ptr = cJSON_Print(root);
    cJSON_Delete(root);

    return json_ptr;
}



void ai_agent_start(void)
{
    char request[REQ_JSON_LEN] = {'\0'};
    char start_url[JSON_URL_LEN] = {0};
    char auth_header[BASE64_BUFFER_LEN] = {0};
    char credentials[128] = {0};
    char base64_creds[BASE64_BUFFER_LEN] = {0};

    printf("Starting AI Agent...\n");

    snprintf(start_url, JSON_URL_LEN, "%s/%s/join", AGORA_AI_AGENT_API_URL, g_app.app_id);
    snprintf(credentials, sizeof(credentials), "%s:%s", AGORA_API_KEY, AGORA_API_SECRET);
    base64_encode(credentials, base64_creds, strlen(credentials));
    snprintf(auth_header, sizeof(auth_header), "Basic %s", base64_creds);

    esp_event_loop_create_default();

    esp_http_client_config_t config = {
        .url              = start_url,
        .event_handler    = _http_event_handler,
        .timeout_ms       = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("Failed to initialize HTTP client for AI Agent start\n");
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Authorization", auth_header);

    char *json_buff = _build_agora_agent_join_json();
    if (json_buff == NULL) {
        printf("Failed to build JSON request\n");
        esp_http_client_cleanup(client);
        return;
    }

    snprintf(request, REQ_JSON_LEN, "%s", json_buff);
    free(json_buff);

    esp_err_t err = esp_http_client_open(client, strlen(request));
    if (err != ESP_OK) {
        printf("Failed to open HTTP connection: %s\n", esp_err_to_name(err));
    } else {
        int wlen = esp_http_client_write(client, request, strlen(request));
        if (wlen < 0) {
            printf("HTTP client write failed\n");
        } else {
            err = esp_http_client_perform(client);
            if (err == ESP_OK) {
                int status_code = esp_http_client_get_status_code(client);
                if (status_code == 200) {
                    printf("AI Agent started successfully\n");
                } else {
                    printf("AI Agent start failed with HTTP status %d\n", status_code);
                }
            } else {
                printf("HTTP request failed: %s\n", esp_err_to_name(err));
            }
        }
    }

    esp_http_client_cleanup(client);
}

/* Stop Agora AI Agent */
void ai_agent_stop(void)
{
    char stop_url[JSON_URL_LEN] = {0};
    char auth_header[BASE64_BUFFER_LEN] = {0};
    char credentials[128] = {0};
    char base64_creds[BASE64_BUFFER_LEN] = {0};

    if (strlen(g_app.agent_id) == 0) {
        printf("No agent_id found\n");
        return;
    }

    printf("Stopping AI Agent...\n");

    snprintf(stop_url, JSON_URL_LEN, "%s/%s/agents/%s/leave",
             AGORA_AI_AGENT_API_URL, g_app.app_id, g_app.agent_id);
    snprintf(credentials, sizeof(credentials), "%s:%s", AGORA_API_KEY, AGORA_API_SECRET);
    base64_encode(credentials, base64_creds, strlen(credentials));
    snprintf(auth_header, sizeof(auth_header), "Basic %s", base64_creds);

    esp_http_client_config_t config = {
        .url              = stop_url,
        .event_handler    = _http_event_handler,
        .timeout_ms       = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("Failed to initialize HTTP client for AI Agent stop\n");
        return;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code == 200) {
            printf("AI Agent stopped successfully\n");
            memset(g_app.agent_id, 0, sizeof(g_app.agent_id));
        } else {
            printf("AI Agent stop failed with status %d\n", status_code);
        }
    } else {
        printf("HTTP request failed: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}
