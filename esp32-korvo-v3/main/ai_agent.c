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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "common.h"

extern const char app_config_json_start[] asm("_binary_app_config_h_start");
extern const char app_config_json_end[] asm("_binary_app_config_h_end");


#define JSON_URL_LEN           512
#define REQ_JSON_LEN           8192
#define MAX_HTTP_OUTPUT_BUFFER 4096
#define BASE64_BUFFER_LEN      256
#define START_RETRY_DELAY_MS   800
#define CHANNEL_SUFFIX_LEN     14

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

static char *_duplicate_embedded_app_config(void)
{
    size_t config_len = (size_t)(app_config_json_end - app_config_json_start);
    char *config_copy = (char *)calloc(config_len + 1, sizeof(char));
    if (config_copy == NULL) {
        return NULL;
    }

    memcpy(config_copy, app_config_json_start, config_len);
    config_copy[config_len] = '\0';
    return config_copy;
}

static int _parse_uid_string(const cJSON *uid_item, uint32_t *out_uid)
{
    char *endptr = NULL;
    unsigned long parsed = 0;

    if (!cJSON_IsString(uid_item) || uid_item->valuestring == NULL || out_uid == NULL) {
        return -1;
    }

    parsed = strtoul(uid_item->valuestring, &endptr, 10);
    if (endptr == uid_item->valuestring || *endptr != '\0') {
        return -1;
    }

    *out_uid = (uint32_t)parsed;
    return 0;
}

static int _build_randomized_channel_name(const char *base_channel, char *out_channel, size_t out_channel_len)
{
    uint32_t random_value = 0;
    uint32_t tick_value = 0;
    int suffix_len = 0;
    int base_len = 0;

    if (base_channel == NULL || out_channel == NULL || out_channel_len <= CHANNEL_SUFFIX_LEN) {
        return -1;
    }

    random_value = esp_random();
    tick_value = (uint32_t)xTaskGetTickCount();
    suffix_len = snprintf(out_channel, out_channel_len, "_%08lx_%04lx",
                          (unsigned long)random_value,
                          (unsigned long)(tick_value & 0xFFFF));
    if (suffix_len <= 0 || (size_t)suffix_len >= out_channel_len) {
        return -1;
    }

    base_len = (int)strlen(base_channel);
    if ((size_t)(base_len + suffix_len + 1) > out_channel_len) {
        base_len = (int)(out_channel_len - (size_t)suffix_len - 1);
    }

    memcpy(out_channel, base_channel, (size_t)base_len);
    snprintf(out_channel + base_len, out_channel_len - (size_t)base_len, "_%08lx_%04lx",
             (unsigned long)random_value,
             (unsigned long)(tick_value & 0xFFFF));
    return 0;
}

static int _load_output_audio_codec(const cJSON *properties)
{
    const cJSON *parameters = NULL;
    const cJSON *output_audio_codec = NULL;

    parameters = cJSON_GetObjectItemCaseSensitive(properties, "parameters");
    output_audio_codec = cJSON_GetObjectItemCaseSensitive(parameters, "output_audio_codec");

    if (!cJSON_IsString(output_audio_codec) || output_audio_codec->valuestring == NULL) {
        printf("app_config.h is missing properties.parameters.output_audio_codec\n");
        return -1;
    }

    if (strcmp(output_audio_codec->valuestring, "PCMU") == 0) {
        g_app.rtc_audio_codec_type = 4;  // AUDIO_CODEC_TYPE_G711U
        g_app.pcm_sample_rate = 8000;
        g_app.pcm_data_len = 320;
        return 0;
    }

    if (strcmp(output_audio_codec->valuestring, "G722") == 0) {
        g_app.rtc_audio_codec_type = 2;  // AUDIO_CODEC_TYPE_G722
        g_app.pcm_sample_rate = 16000;
        g_app.pcm_data_len = 640;
        return 0;
    }

    printf("Unsupported output_audio_codec: %s\n", output_audio_codec->valuestring);
    return -1;
}

int ai_agent_load_config(void)
{
    char *config_json = NULL;
    char randomized_channel[AI_AGENT_CHANNEL_LEN] = {0};
    cJSON *root = NULL;
    cJSON *properties = NULL;
    cJSON *name = NULL;
    cJSON *channel = NULL;
    cJSON *agent_rtc_uid = NULL;
    cJSON *remote_rtc_uids = NULL;
    cJSON *remote_rtc_uid = NULL;

    config_json = _duplicate_embedded_app_config();
    if (config_json == NULL) {
        printf("Failed to allocate app_config JSON buffer\n");
        return -1;
    }

    root = cJSON_Parse(config_json);
    if (root == NULL) {
        printf("Embedded app_config.h is not valid JSON\n");
        free(config_json);
        return -1;
    }

    name = cJSON_GetObjectItemCaseSensitive(root, "name");
    properties = cJSON_GetObjectItemCaseSensitive(root, "properties");
    channel = cJSON_GetObjectItemCaseSensitive(properties, "channel");
    agent_rtc_uid = cJSON_GetObjectItemCaseSensitive(properties, "agent_rtc_uid");
    remote_rtc_uids = cJSON_GetObjectItemCaseSensitive(properties, "remote_rtc_uids");
    remote_rtc_uid = cJSON_IsArray(remote_rtc_uids) ? cJSON_GetArrayItem(remote_rtc_uids, 0) : NULL;

    if (!cJSON_IsString(name) || name->valuestring == NULL ||
        !cJSON_IsString(channel) || channel->valuestring == NULL ||
        _parse_uid_string(agent_rtc_uid, &g_app.agent_rtc_uid) != 0 ||
        _parse_uid_string(remote_rtc_uid, &g_app.remote_rtc_uid) != 0 ||
        _load_output_audio_codec(properties) != 0) {
        printf("app_config.h is missing required fields: name/channel/agent_rtc_uid/remote_rtc_uids[0]/parameters.output_audio_codec\n");
        cJSON_Delete(root);
        free(config_json);
        return -1;
    }

    strncpy(g_app.agent_name, name->valuestring, AI_AGENT_NAME_LEN - 1);
    g_app.agent_name[AI_AGENT_NAME_LEN - 1] = '\0';
    if (_build_randomized_channel_name(channel->valuestring, randomized_channel, sizeof(randomized_channel)) != 0) {
        printf("Failed to build randomized channel name from app_config.h\n");
        cJSON_Delete(root);
        free(config_json);
        return -1;
    }
    strncpy(g_app.channel_name, randomized_channel, AI_AGENT_CHANNEL_LEN - 1);
    g_app.channel_name[AI_AGENT_CHANNEL_LEN - 1] = '\0';

    printf("Loaded agent config: name=%s, base_channel=%s, runtime_channel=%s, agent_rtc_uid=%lu, remote_rtc_uid=%lu, sample_rate=%d, frame_len=%d\n",
           g_app.agent_name,
           channel->valuestring,
           g_app.channel_name,
           (unsigned long)g_app.agent_rtc_uid,
           (unsigned long)g_app.remote_rtc_uid,
           g_app.pcm_sample_rate,
           g_app.pcm_data_len);

    cJSON_Delete(root);
    free(config_json);
    return 0;
}

static int _parse_agora_agent_response(const char *resp_data, const int data_len)
{
    if (data_len == 0) {
        return -1;
    }

    printf("AI Agent response body (%d bytes): %.*s\n", data_len, data_len, resp_data);

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
            if (evt->data_len > 0) {
                printf("HTTP_EVENT_ON_DATA (%d bytes): %.*s\n", evt->data_len, evt->data_len, (char *)evt->data);
            }
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
    char *config_json = NULL;
    char *json_ptr = NULL;
    cJSON *root = NULL;
    cJSON *properties = NULL;
    cJSON *channel = NULL;

    config_json = _duplicate_embedded_app_config();
    if (config_json == NULL) {
        return NULL;
    }

    root = cJSON_Parse(config_json);
    if (root == NULL) {
        printf("Embedded app_config.h is not valid JSON\n");
        free(config_json);
        return NULL;
    }

    properties = cJSON_GetObjectItemCaseSensitive(root, "properties");
    channel = cJSON_GetObjectItemCaseSensitive(properties, "channel");
    if (!cJSON_IsString(channel) || channel->valuestring == NULL) {
        printf("Embedded app_config.h is missing properties.channel\n");
        cJSON_Delete(root);
        free(config_json);
        return NULL;
    }
    if (!cJSON_SetValuestring(channel, g_app.channel_name)) {
        printf("Failed to update request JSON channel\n");
        cJSON_Delete(root);
        free(config_json);
        return NULL;
    }

    json_ptr = cJSON_PrintUnformatted(root);

    cJSON_Delete(root);
    free(config_json);

    return json_ptr;
}

static void _build_auth_header(char *auth_header, size_t auth_header_len)
{
    char credentials[128] = {0};
    char base64_creds[BASE64_BUFFER_LEN] = {0};

    snprintf(credentials, sizeof(credentials), "%s:%s", AGORA_API_KEY, AGORA_API_SECRET);
    base64_encode(credentials, base64_creds, strlen(credentials));
    snprintf(auth_header, auth_header_len, "Basic %s", base64_creds);
}

static int _query_running_agent_id(char *out_agent_id, size_t out_agent_id_len)
{
    char query_url[JSON_URL_LEN] = {0};
    char auth_header[BASE64_BUFFER_LEN] = {0};
    char *resp_buf = NULL;

    snprintf(query_url, JSON_URL_LEN, "%s/%s/agents?channel=%s&state=2&limit=20",
             AGORA_AI_AGENT_API_URL, g_app.app_id, g_app.channel_name);
    _build_auth_header(auth_header, sizeof(auth_header));

    resp_buf = (char *)calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
    if (resp_buf == NULL) {
        printf("Failed to allocate response buffer for agent query\n");
        return -1;
    }

    esp_http_client_config_t config = {
        .url               = query_url,
        .timeout_ms        = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("Failed to initialize HTTP client for agent query\n");
        free(resp_buf);
        return -1;
    }

    esp_http_client_set_method(client, HTTP_METHOD_GET);
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        printf("Agent query request failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    int rlen = esp_http_client_read_response(client, resp_buf, MAX_HTTP_OUTPUT_BUFFER - 1);
    if (rlen > 0) {
        resp_buf[rlen] = '\0';
    } else {
        resp_buf[0] = '\0';
    }
    printf("Agent query status=%d, body=%s\n", status_code, resp_buf);

    if (status_code != 200) {
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    cJSON *root = cJSON_Parse(resp_buf);
    if (root == NULL) {
        printf("Agent query JSON parse error\n");
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    cJSON *list = cJSON_GetObjectItemCaseSensitive(data, "list");
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) <= 0) {
        printf("Agent query returned no running agent in channel %s\n", g_app.channel_name);
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    cJSON *first = cJSON_GetArrayItem(list, 0);
    cJSON *agent_id = cJSON_GetObjectItemCaseSensitive(first, "agent_id");
    if (!cJSON_IsString(agent_id) || agent_id->valuestring == NULL) {
        printf("Agent query missing agent_id\n");
        cJSON_Delete(root);
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    strncpy(out_agent_id, agent_id->valuestring, out_agent_id_len - 1);
    out_agent_id[out_agent_id_len - 1] = '\0';
    cJSON_Delete(root);
    esp_http_client_cleanup(client);
    free(resp_buf);
    return 0;
}

static int _stop_agent_by_id(const char *agent_id)
{
    char stop_url[JSON_URL_LEN] = {0};
    char auth_header[BASE64_BUFFER_LEN] = {0};
    char *resp_buf = NULL;

    if (agent_id == NULL || strlen(agent_id) == 0) {
        printf("Invalid agent_id for stop\n");
        return -1;
    }

    snprintf(stop_url, JSON_URL_LEN, "%s/%s/agents/%s/leave", AGORA_AI_AGENT_API_URL, g_app.app_id, agent_id);
    _build_auth_header(auth_header, sizeof(auth_header));

    resp_buf = (char *)calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
    if (resp_buf == NULL) {
        printf("Failed to allocate response buffer for AI Agent stop\n");
        return -1;
    }

    esp_http_client_config_t config = {
        .url               = stop_url,
        .timeout_ms        = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        printf("Failed to initialize HTTP client for AI Agent stop\n");
        free(resp_buf);
        return -1;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Authorization", auth_header);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        printf("HTTP request failed: %s\n", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(resp_buf);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    int rlen = esp_http_client_read_response(client, resp_buf, MAX_HTTP_OUTPUT_BUFFER - 1);
    if (rlen > 0) {
        resp_buf[rlen] = '\0';
    } else {
        resp_buf[0] = '\0';
    }
    printf("AI Agent stop status=%d, body=%s\n", status_code, resp_buf);

    if (status_code == 200) {
        printf("AI Agent stopped successfully: %s\n", agent_id);
        memset(g_app.agent_id, 0, sizeof(g_app.agent_id));
        esp_http_client_cleanup(client);
        free(resp_buf);
        return 0;
    }

    printf("AI Agent stop failed with status %d\n", status_code);
    esp_http_client_cleanup(client);
    free(resp_buf);
    return -1;
}



int ai_agent_start(void)
{
    char start_url[JSON_URL_LEN] = {0};
    char auth_header[BASE64_BUFFER_LEN] = {0};
    char *request = NULL;
    int start_ok = -1;
    int attempt = 0;

    printf("Starting AI Agent...\n");

    snprintf(start_url, JSON_URL_LEN, "%s/%s/join", AGORA_AI_AGENT_API_URL, g_app.app_id);
    _build_auth_header(auth_header, sizeof(auth_header));

    esp_event_loop_create_default();

    esp_http_client_config_t config = {
        .url              = start_url,
        .event_handler    = _http_event_handler,
        .timeout_ms       = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    request = _build_agora_agent_join_json();
    if (request == NULL) {
        printf("Failed to build JSON request\n");
        return -1;
    }

    if (strlen(request) >= REQ_JSON_LEN) {
        printf("JSON request too large (%d bytes), max is %d\n", (int)strlen(request), REQ_JSON_LEN - 1);
        free(request);
        return -1;
    }
    printf("\n========== AI Agent START Request Body (%d bytes) ==========\n%s\n========== END START Request Body ==========\n",
           (int)strlen(request), request);

    for (attempt = 0; attempt < 2; ++attempt) {
        bool retry_after_409 = false;
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            printf("Failed to initialize HTTP client for AI Agent start\n");
            free(request);
            return -1;
        }

        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "Authorization", auth_header);

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
                        start_ok = 0;
                    } else if (status_code == 409 && attempt == 0) {
                        char running_agent_id[128] = {0};
                        printf("AI Agent start got 409. Querying running agent and retrying...\n");
                        if (strlen(g_app.agent_id) > 0) {
                            strncpy(running_agent_id, g_app.agent_id, sizeof(running_agent_id) - 1);
                            running_agent_id[sizeof(running_agent_id) - 1] = '\0';
                            printf("Using agent_id from 409 response: %s\n", running_agent_id);
                        } else if (_query_running_agent_id(running_agent_id, sizeof(running_agent_id)) != 0) {
                            printf("Failed to query running agent_id from server\n");
                        }

                        if (strlen(running_agent_id) > 0 &&
                            _stop_agent_by_id(running_agent_id) == 0) {
                            vTaskDelay(pdMS_TO_TICKS(START_RETRY_DELAY_MS));
                            printf("Retrying AI Agent start after stopping agent %s\n", running_agent_id);
                            retry_after_409 = true;
                        } else {
                            printf("Failed to recover from 409 (query/stop failed)\n");
                            esp_http_client_cleanup(client);
                            break;
                        }
                    } else {
                        printf("AI Agent start failed with HTTP status %d\n", status_code);
                    }
                } else {
                    printf("HTTP request failed: %s\n", esp_err_to_name(err));
                }
            }
        }

        esp_http_client_cleanup(client);
        if (start_ok == 0) {
            break;
        }
        if (!retry_after_409) {
            break;
        }
    }
    free(request);
    return start_ok;
}

/* Stop Agora AI Agent */
void ai_agent_stop(void)
{
    if (strlen(g_app.agent_id) == 0) {
        printf("No agent_id found\n");
        return;
    }

    printf("Stopping AI Agent...\n");
    (void)_stop_agent_by_id(g_app.agent_id);
}
