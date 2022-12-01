#define _POSIX_C_SOURCE 1
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/mman.h>
#include <curl/curl.h>
#include <cJSON.h>
#include "id3v2lib.h"

#include "urlencode.h"
#include "token.h"

#define UNUSED(var) do {(void)var;} while (0)

// Redefining disgusting names
#define json_getobj        cJSON_GetObjectItemCaseSensitive
#define json_foreach       cJSON_ArrayForEach
#define json_isstr         cJSON_IsString
#define json_isnum         cJSON_IsNumber
#define json_parse         cJSON_Parse
#define json_print         cJSON_Print
#define json_create_object cJSON_CreateObject
#define json_create_array  cJSON_CreateArray
#define json_create_string cJSON_CreateString
#define json_add_to_object cJSON_AddItemToObject
#define json_delete        cJSON_Delete

static inline size_t
fsize(FILE *file)
{
    size_t size;
    size_t pos = ftell(file);

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, pos, SEEK_SET);

    return size;
}

static inline char*
gen_str(char *buf, size_t len)
{
    size_t i;

    srand(time(NULL));

    for (i = 0; i < len; ++i) {
        int r = rand() % 52;

        buf[i] = r + (r < 26 ? 'A' : 'a' - 26);
    }

    buf[i] = '\0';

    return buf;
}

static inline bool
file_copy(const char *to, const char *from)
{
    int c;
    FILE *to_file = fopen(to, "w");
    FILE *from_file = fopen(from, "r");

    for (; (c = fgetc(from_file)) != EOF;)
        fputc(c, to_file);

    return true;
}

typedef struct fw_artist {
    size_t id;
    char *name;
} fw_artist;

typedef struct fw_album {
    size_t id;
    char *name;
} fw_album;

typedef struct fw_track {
    size_t id;
    char *name;
} fw_track;

typedef struct fw_library {
    char *id;
    char *name;
    char *desc;
} fw_library;

typedef struct fw_channel {
    char *id;
    char *name;
    char *username;
    char *descx;
} fw_channel;

typedef struct fw_language {
    char *value;
    char *label;
} fw_language;

typedef struct fw_subcategory {
    char *label;
    struct fw_subcategory *next;
} fw_subcategory;

typedef struct fw_category {
    char *value;
    char *label;

    fw_subcategory *sub;
} fw_category;

typedef struct fw_attachment {
    char *id;
    char *mime;
} fw_attachment;

typedef enum fw_request_type {
    FW_NOTHING,
    FW_ARTISTS,
    FW_ALBUMS,
    FW_TRACKS,
    FW_LIBRARIES,
    FW_CHANNELS,
    FW_METADATA,
    FW_ATTACHMENTS,
} fw_request_type;

typedef enum fw_metadata_type {
    FW_META_NOTHING,
    FW_META_LANGUAGE,
    FW_META_CATEGORY,
} fw_metadata_type;

typedef struct funkctx {
    FILE *devnull;

    CURL *curl;
    char url[256];

    char client_id[512];
    char client_secret[512];
    char scope[1024];
    char redirect_uri[256];
    char auth_url[3*1024];

    char user_token[512];
    char error[CURL_ERROR_SIZE];

    fw_request_type result_type;
    fw_metadata_type metadata_type;

    struct list {
        union {
            fw_artist     artist;
            fw_album      album;
            fw_track      track;
            fw_library    library;
            fw_channel    channel;
            fw_language   language;
            fw_category   category;
            fw_attachment attachment;
        };
        struct list *next;
    } *results;
} funkctx;

typedef struct fw_track_tags {
    char track_file[512];
    char cover_file[512];

    char artist[128];
    char album[128];
    char title[128];
    char genre[128];
    char track[64];
    char year[64];
} fw_track_tags;


funkctx*
fw_init(char *scheme, const char *server)
{
    funkctx *ctx = calloc(sizeof(funkctx), 1); // Don't forget to free

    if (!ctx)
        return NULL;

    ctx->curl = curl_easy_init();
    if (!ctx->curl) {
        free(ctx);
        return NULL;
    }

    ctx->devnull = fopen("/dev/null", "r+"); // Don't forget to close
    snprintf(ctx->url, sizeof(ctx->url), "%s://%s", scheme, server);

    curl_easy_setopt(ctx->curl, CURLOPT_DEFAULT_PROTOCOL, scheme);
    curl_easy_setopt(ctx->curl, CURLOPT_URL, server);
    //curl_easy_setopt(ctx->curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_UNRESTRICTED_AUTH, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_ERRORBUFFER, ctx->error);

    if (curl_easy_perform(ctx->curl) != CURLE_OK) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

bool
print_results(funkctx *ctx)
{
    struct list *node;

    for (node = ctx->results; node; node = node->next) {
        switch (ctx->result_type) {
            case FW_NOTHING:
                printf("There are no results\n");
                break;

            case FW_ARTISTS:
                printf("%zu. %s\n", node->artist.id, node->artist.name);
                break;

            case FW_ALBUMS:
                printf("%zu. %s\n", node->album.id, node->album.name);
                break;

            case FW_TRACKS:
                printf("%zu. %s\n", node->track.id, node->track.name);
                break;

            case FW_LIBRARIES:
                printf("%s (%s)\n", node->library.name, node->library.id);
                printf("    %s\n", node->library.desc);
                break;

            case FW_CHANNELS:
                printf("%s (@%s/%s)\n", node->channel.name, node->channel.username, node->channel.id);
                break;

            case FW_METADATA:
                switch (ctx->metadata_type) {
                    case FW_META_LANGUAGE:
                        printf("%s\n", node->language.label);
                        break;
                    case FW_META_CATEGORY: {
                        fw_subcategory *sub;

                        printf("%s\n", node->category.label);

                        for (sub = node->category.sub; sub; sub = sub->next)
                            printf("    %s\n", sub->label);

                        break;
                    }

                    case FW_META_NOTHING:
                    default:
                        break;
                }
                break;

            case FW_ATTACHMENTS:
                printf("%s (%s)\n", ctx->results->attachment.mime, ctx->results->attachment.id);
                break;
        }
    }

    return true;
}

const char*
fw_error_str(funkctx *ctx)
{
    return ctx->error;
}

bool
clean_results(funkctx *ctx)
{
    struct list *node, *next;

    for (node = ctx->results; node; node = next) {
        next = node->next;

        switch (ctx->result_type) {
            case FW_NOTHING:
                fprintf(stderr, "Impossible case. Check the code for memory leaking.\n");
                return true;

            case FW_ARTISTS:
                free(node->artist.name);
                break;

            case FW_ALBUMS:
                free(node->album.name);
                break;

            case FW_TRACKS:
                free(node->track.name);
                break;

            case FW_LIBRARIES:
                free(node->library.id);
                free(node->library.name);
                free(node->library.desc);
                break;

            case FW_CHANNELS:
                free(node->channel.id);
                free(node->channel.name);
                free(node->channel.username);
                break;

            case FW_METADATA:
                switch (ctx->metadata_type) {
                    case FW_META_LANGUAGE:
                        free(node->language.value);
                        free(node->language.label);
                        break;
                    case FW_META_CATEGORY: {
                        fw_subcategory *sub, *sub_next;

                        free(node->category.value);
                        free(node->category.label);

                        for (sub = node->category.sub; sub; sub = sub_next) {
                            sub_next = sub->next;

                            free(sub->label);
                            free(sub);
                        }

                        break;
                    }

                    case FW_META_NOTHING:
                    default:
                        break;
                }

            case FW_ATTACHMENTS:
                free(node->attachment.id);
                free(node->attachment.mime);
        }

        free(node);
    }

    ctx->result_type = FW_NOTHING;
    ctx->results = NULL;

    return true;
}

bool
fw_get_metadata(funkctx *ctx, fw_metadata_type type)
{
    CURLcode rc;
    FILE *resp_file = tmpfile(); // Don't forget to close
    char *resp;
    size_t resp_sz;
    cJSON *json, *result, *results;
    char request[1024] = "/api/v1/channels/metadata-choices";
    struct list **resultsp = &ctx->results;
    struct curl_slist *headers = NULL;
    char *scheme = NULL;

    clean_results(ctx);
    ctx->result_type = FW_METADATA;
    ctx->metadata_type = type;

    curl_easy_getinfo(ctx->curl, CURLINFO_SCHEME, &scheme);

    // Don't send an Authorization header if it is not a https connection
    if (*ctx->user_token && scheme && !strcasecmp(scheme, "https"))
        headers = curl_slist_append(headers, strcat((char[512]){"Authorization: Bearer "}, ctx->user_token));

    curl_easy_setopt(ctx->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, request);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_sz = fsize(resp_file);
    resp = mmap(NULL, resp_sz, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // file is mapped. Don't forget to unmap
    fclose(resp_file);

    // TODO: check the json object before parsing
    json = json_parse(resp); // json tree is allocated. Don't forget to free
    munmap(resp, resp_sz);

    switch (type) {
        case FW_META_LANGUAGE:
            results = json_getobj(json, "language");
            break;

        case FW_META_CATEGORY:
            results = json_getobj(json, "itunes_category");
            break;

        case FW_META_NOTHING:
        default:
            json_delete(json);
            return false;
    }

    json_foreach (result, results) {
        char *value = json_getobj(result, "value")->valuestring;
        char *label = json_getobj(result, "label")->valuestring;

        if (ctx->metadata_type == FW_META_LANGUAGE) {
            *resultsp = calloc(sizeof(**resultsp), 1);

            (*resultsp)->language.value = malloc(strlen(value) + 1);
            (*resultsp)->language.label = malloc(strlen(label) + 1);

            strcpy((*resultsp)->language.value, value);
            strcpy((*resultsp)->language.label, label);
        }
        else if (ctx->metadata_type == FW_META_CATEGORY) {
            cJSON *sub;
            fw_subcategory **sub_next;

            *resultsp = calloc(sizeof(**resultsp), 1);

            (*resultsp)->category.value = malloc(strlen(value) + 1);
            (*resultsp)->category.label = malloc(strlen(label) + 1);

            strcpy((*resultsp)->category.value, value);
            strcpy((*resultsp)->category.label, label);

            sub_next = &(*resultsp)->category.sub;

            json_foreach (sub, json_getobj(result, "children")) {
                *sub_next = calloc(sizeof(**sub_next), 1);
                (*sub_next)->label = malloc(strlen(sub->valuestring) + 1);
                strcpy((*sub_next)->label, sub->valuestring);
                sub_next = &(*sub_next)->next;
            }
        }

        resultsp = &(*resultsp)->next;
    }

    json_delete(json);

    return true;
}

bool
fw_get(funkctx *ctx, fw_request_type req_type, const char *search)
{
    CURLcode rc;
    FILE *resp_file = tmpfile(); // Don't forget to close
    char *resp;
    size_t resp_sz;
    cJSON *json, *result;
    char request[1024];
    struct list **resultsp = &ctx->results;
    struct curl_slist *headers = NULL;
    char *scheme = NULL;

    clean_results(ctx);
    ctx->result_type = req_type;

    switch (req_type) {
        case FW_ARTISTS:
            strcpy(request, "/api/v1/artists?ordering=name&page=1&page_size=10&content_category=music");
            break;

        case FW_ALBUMS:
            strcpy(request, "/api/v1/albums?ordering=title&page=1&page_size=10&content_category=music");
            break;

        case FW_TRACKS:
            strcpy(request, "/api/v1/tracks?ordering=title&page=1&page_size=10&content_category=music");
            break;

        case FW_LIBRARIES:
            strcpy(request, "/api/v1/libraries?page=1&page_size=10&scope=me");
            break;

        case FW_CHANNELS:
            strcpy(request, "/api/v1/channels?ordering=creation_date&page=1&page_size=10&subscribed=false&external=false");
            break;

        case FW_NOTHING:
        default:
            ctx->result_type = FW_NOTHING;
            return false;
    }

    // Add a 'q' request if it's needed
    if (search && *search != '\0')
        snprintf(request + strlen(request), sizeof(request) - strlen(request),
                 "&q=%s", url_encode(search, (char[3*sizeof(request)]){'\0'}));

    curl_easy_getinfo(ctx->curl, CURLINFO_SCHEME, &scheme);

    // Don't send an Authorization header if it is not a https connection
    if (*ctx->user_token && scheme && !strcasecmp(scheme, "https"))
        headers = curl_slist_append(headers, strcat((char[512]){"Authorization: Bearer "}, ctx->user_token));

    curl_easy_setopt(ctx->curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, request);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_sz = fsize(resp_file);
    resp = mmap(NULL, resp_sz, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // file is mapped. Don't forget to unmap
    fclose(resp_file);

    // TODO: check the json object before parsing
    json = json_parse(resp); // json tree is allocated. Don't forget to free
    munmap(resp, resp_sz);

    json_foreach (result, json_getobj(json, "results")) {
        switch (req_type) {
            case FW_ARTISTS: {
                int id      = json_getobj(result, "id")->valueint;
                char *title = json_getobj(result, "name")->valuestring;

                *resultsp = calloc(sizeof(**resultsp), 1);

                (*resultsp)->artist.id = id;
                (*resultsp)->artist.name = malloc(strlen(title) + 1);

                strcpy((*resultsp)->artist.name, title);

                break;
            }

            case FW_ALBUMS: {
                int id      = json_getobj(result, "id")->valueint;
                char *title = json_getobj(result, "title")->valuestring;

                *resultsp = calloc(sizeof(**resultsp), 1);

                (*resultsp)->album.id = id;
                (*resultsp)->album.name = malloc(strlen(title) + 1);

                strcpy((*resultsp)->album.name, title);

                break;
            }

            case FW_TRACKS: {
                int id      = json_getobj(result, "id")->valueint;
                char *title = json_getobj(result, "title")->valuestring;

                *resultsp = calloc(sizeof(**resultsp), 1);

                (*resultsp)->track.id = id;
                (*resultsp)->track.name = malloc(strlen(title) + 1);

                strcpy((*resultsp)->track.name, title);

                break;
            }

            case FW_LIBRARIES: {
                char *id = json_getobj(result, "uuid")->valuestring;
                char *name = json_getobj(result, "name")->valuestring;
                char *desc = json_getobj(result, "description")->valuestring;

                *resultsp = calloc(sizeof(**resultsp), 1);

                (*resultsp)->library.id = malloc(strlen(id) + 1);
                (*resultsp)->library.name = malloc(strlen(name) + 1);
                (*resultsp)->library.desc = malloc(strlen(desc) + 1);

                strcpy((*resultsp)->library.id, id);
                strcpy((*resultsp)->library.name, name);
                strcpy((*resultsp)->library.desc, desc);

                break;
            }

            case FW_CHANNELS: {
                char *id = json_getobj(result, "uuid")->valuestring;
                char *name = json_getobj(json_getobj(result, "actor"), "name")->valuestring;
                char *username = json_getobj(json_getobj(result, "actor"), "preferred_username")->valuestring;

                *resultsp = calloc(sizeof(**resultsp), 1);

                (*resultsp)->channel.id = malloc(strlen(id) + 1);
                (*resultsp)->channel.name = malloc(strlen(name) + 1);
                (*resultsp)->channel.username = malloc(strlen(username) + 1);

                strcpy((*resultsp)->channel.id, id);
                strcpy((*resultsp)->channel.name, name);
                strcpy((*resultsp)->channel.username, username);

                break;
            }

            case FW_NOTHING:
            default:
                fprintf(stderr, "Impossible case\n"); // Handled before
                exit(1);
        }

        resultsp = &(*resultsp)->next;
    }

    json_delete(json);

    return true;
}

bool
fw_upload_track(funkctx *ctx, const char *lib_id, fw_track_tags *tags)
{
    CURLcode rc;

    size_t resp_size;
    FILE *resp_file = tmpfile(); // Don't forget to close
    char *resp;

    size_t post_size;
    FILE *post_file = tmpfile(); // Don't forget to close
    char *post_buf;

    size_t mp3_size;
    const uint8_t *mp3;

    //cJSON *json, *metadata;
    struct curl_slist *headers = NULL;

    char boundary[16];

    ID3v2_tag *tag = new_tag(); // Dont forget to free

    tag_set_artist(tags->artist, 3, tag);
    tag_set_album(tags->album, 3, tag);
    tag_set_title(tags->title, 3, tag);
    tag_set_genre(tags->genre, 3, tag);
    tag_set_track(tags->track, 3, tag);
    tag_set_year(tags->year, 3, tag);
    tag_set_album_cover(tags->cover_file, tag);

    {   // Making mp3id3v2 taged pointer. Don't forget to unmap ``mp3``
        char mp3id_name[] = "/tmp/XXXXXX";
        FILE *mp3id_file = fdopen(mkstemp(mp3id_name), "r+");

        file_copy(mp3id_name, tags->track_file);
        set_tag(mp3id_name, tag);
        free_tag(tag);
        unlink(mp3id_name);

        mp3_size = fsize(mp3id_file);
        mp3 = mmap(NULL, mp3_size, PROT_READ, MAP_PRIVATE, fileno(mp3id_file), 0);
        fclose(mp3id_file);
    }

    gen_str(boundary, sizeof(boundary) - 1);

    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"library\"\r\n\r\n");
    fprintf(post_file, "%s\r\n", lib_id);
    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"import_reference\"\r\n\r\n");
    fprintf(post_file, "Import launched via libfunkwhale\r\n");
    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"source\"\r\n\r\n");
    fprintf(post_file, "upload://filename.mp3\r\n");
    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"import_status\"\r\n\r\n");
    fprintf(post_file, "pending\r\n");
    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"import_metadata\"\r\n\r\n");
    fprintf(post_file, "{\"title\": \"%s\", \"position\": %d}\r\n", tags->title, atoi(tags->track)); // TODO
    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"audio_file\"; filename=\"filename.mp3\"\"\r\n");
    fprintf(post_file, "Content-Type: audio/mpeg\r\n");
    fprintf(post_file, "Content-Length: %zu\r\n", mp3_size);
    fprintf(post_file, "\r\n");
    fwrite(mp3, mp3_size, 1, post_file);
    fprintf(post_file, "\r\n");
    fprintf(post_file, "--%s--\r\n\r\n", boundary);

    munmap((void*)mp3, mp3_size);

    {   // set headers. Don't forget to free ``headers``
        char *scheme = NULL;

        curl_easy_getinfo(ctx->curl, CURLINFO_SCHEME, &scheme);

        if (*ctx->user_token && scheme && !strcasecmp(scheme, "https"))
            headers = curl_slist_append(headers, strcat((char[512]){"Authorization: Bearer "}, ctx->user_token));

        headers = curl_slist_append(headers, strcat((char[512]){"Content-Type: multipart/form-data; boundary="}, boundary));
    }

    post_size = fsize(post_file);
    post_buf = mmap(NULL, post_size, PROT_READ, MAP_PRIVATE, fileno(post_file), 0); // Don't forget to unmap
    fclose(post_file);

    curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, "/api/v1/uploads");
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, post_size);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post_buf);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, 0);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    munmap(post_buf, post_size);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_size = fsize(resp_file);
    resp = mmap(NULL, resp_size, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // Don't forget to unmap
    fclose(resp_file);

    //printf("%.*s\n", (int)resp_size, resp);

    munmap(resp, resp_size);

    return true;
}

bool
fw_create_channel(funkctx *ctx, fw_channel *channel)
{
    CURLcode rc;

    FILE *resp_file = tmpfile(); // a file was opened. Don't forget to close
    size_t resp_size;
    char *resp;

    char *post_str;
    cJSON *post;

    struct curl_slist *headers = NULL;
    char *scheme = NULL;

    post = json_create_object(); // json object was allocated. Don't forget to free

    json_add_to_object(post, "name", json_create_string(channel->name));
    json_add_to_object(post, "username", json_create_string(channel->username));
    json_add_to_object(post, "tags", json_create_array());
    json_add_to_object(post, "content_category", json_create_string("music"));
    json_add_to_object(post, "cover", json_create_string(""));
    // TODO: add metadata object

    {
        cJSON *description = json_create_object();

        json_add_to_object(description, "text", json_create_string(channel->descx));
        json_add_to_object(description, "content_type", json_create_string("text/plain"));
        json_add_to_object(post, "description", description);
    }

    post_str = json_print(post);
    json_delete(post);

    curl_easy_getinfo(ctx->curl, CURLINFO_SCHEME, &scheme);

    // Don't send an Authorization header if it is not a https connection
    if (*ctx->user_token && scheme && !strcasecmp(scheme, "https"))
        headers = curl_slist_append(headers, strcat((char[512]){"Authorization: Bearer "}, ctx->user_token));

    headers = curl_slist_append(headers, "Content-Type: application/json"); // Don't forget to free the headers

    curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, "/api/v1/channels");
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, strlen(post_str));
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post_str);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, 0);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    free(post_str);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_size = fsize(resp_file);
    resp = mmap(NULL, resp_size, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // Don't forget to unmap
    fclose(resp_file);

    printf("%.*s\n\n", (int)resp_size, resp);

    munmap(resp, resp_size);

    return true;
}

bool
fw_attach(funkctx *ctx, FILE *file, const char *mime)
{
    CURLcode rc;

    size_t file_size;
    uint8_t *file_buf;

    size_t resp_size;
    FILE *resp_file = tmpfile(); // Don't forget to close
    char *resp;

    size_t post_size;
    FILE *post_file = tmpfile(); // Don't forget to close
    char *post_buf;

    struct curl_slist *headers = NULL;

    char boundary[16];

    clean_results(ctx);
    ctx->result_type = FW_ATTACHMENTS;

    file_size = fsize(file);
    file_buf = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fileno(file), 0);

    gen_str(boundary, sizeof(boundary) - 1);

    fprintf(post_file, "--%s\r\n", boundary);
    fprintf(post_file, "Content-Disposition: form-data; name=\"file\"; filename=\"filename.jpg\"\r\n");
    // fprintf(post_file, "Content-Type: %s\r\n", mime);
    fprintf(post_file, "Content-Length: %zu\r\n", file_size);
    fprintf(post_file, "\r\n");
    fwrite(file_buf, file_size, 1, post_file);
    fprintf(post_file, "\r\n");
    fprintf(post_file, "--%s--\r\n", boundary);
    fprintf(post_file, "\r\n");

    munmap(file_buf, file_size);

    {   // set headers. Don't forget to free ``headers``
        char *scheme = NULL;

        curl_easy_getinfo(ctx->curl, CURLINFO_SCHEME, &scheme);

        if (*ctx->user_token && scheme && !strcasecmp(scheme, "https"))
            headers = curl_slist_append(headers, strcat((char[512]){"Authorization: Bearer "}, ctx->user_token));

        headers = curl_slist_append(headers, strcat((char[512]){"Content-Type: multipart/form-data; boundary="}, boundary));
    }

    post_size = fsize(post_file);
    post_buf = mmap(NULL, post_size, PROT_READ, MAP_PRIVATE, fileno(post_file), 0);
    fclose(post_file);

    curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, "/api/v1/attachments");
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, post_size);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post_buf);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, 0);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    munmap(post_buf, post_size);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_size = fsize(resp_file);
    resp = mmap(NULL, resp_size, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // Don't forget to unmap
    fclose(resp_file);

    {
        char *id;
        char *mime;
        cJSON *json;

        json = json_parse(resp);

        id = json_getobj(json, "uuid")->valuestring;
        mime = json_getobj(json, "mimetype")->valuestring;

        ctx->results = calloc(sizeof(*ctx->results), 1);
        ctx->results->attachment.id = calloc(strlen(id) + 1, 1);
        ctx->results->attachment.mime = calloc(strlen(mime) + 1, 1);

        strcpy(ctx->results->attachment.id, id);
        strcpy(ctx->results->attachment.mime, mime);

        json_delete(json);
    }

    munmap(resp, resp_size);

    return true;
}

bool
fw_get_app_token(funkctx *ctx, const char *app_name, const char *scope)
{
    CURLcode rc;

    FILE *resp_file = tmpfile(); // a file was opened. Don't forget to close
    size_t resp_size;
    char *resp;

    char *post_str;
    const char redirect_uri[] = "urn:ietf:wg:oauth:2.0:oob";
    cJSON *json, *post;
    struct curl_slist *headers = NULL;

    post = json_create_object(); // json object was allocated. Don't forget to free

    json_add_to_object(post, "name", json_create_string(app_name));
    json_add_to_object(post, "redirect_uris", json_create_string(redirect_uri));
    json_add_to_object(post, "scopes", json_create_string(scope));

    post_str = json_print(post); // post string was allocated. Don't forget to free
    json_delete(post);

    headers = curl_slist_append(headers, "Content-Type: application/json"); // Don't forget to free the headers

    curl_easy_setopt(ctx->curl, CURLOPT_POST, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_REQUEST_TARGET, "/api/v1/oauth/apps");
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, resp_file);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, strlen(post_str));
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, post_str);
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, headers);

    rc = curl_easy_perform(ctx->curl);

    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, ctx->devnull);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, 0);
    curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, NULL);
    free(post_str);
    curl_slist_free_all(headers);

    if (rc != CURLE_OK) {
        fclose(resp_file);
        return false;
    }

    resp_size = fsize(resp_file);
    resp = mmap(NULL, resp_size, PROT_READ, MAP_PRIVATE, fileno(resp_file), 0); // Don't forget to unmap
    fclose(resp_file);

    json = json_parse(resp); // Don't forget to free
    munmap(resp, resp_size);

    strncpy(ctx->client_id,     json_getobj(json, "client_id")->valuestring, sizeof(ctx->client_id));
    strncpy(ctx->client_secret, json_getobj(json, "client_secret")->valuestring, sizeof(ctx->client_secret));
    strncpy(ctx->scope,         scope, sizeof(ctx->scope));
    strncpy(ctx->redirect_uri,  redirect_uri, sizeof(ctx->redirect_uri));

    json_delete(json);

    return true;
}

bool
fw_set_app_token(funkctx *ctx, const char *client_id, const char *client_secret, const char *scope, const char *redirect_uri)
{
    strncpy(ctx->client_id, client_id, sizeof(ctx->client_id));
    strncpy(ctx->client_secret, client_secret, sizeof(ctx->client_secret));
    strncpy(ctx->scope, scope, sizeof(ctx->scope));
    strncpy(ctx->redirect_uri, redirect_uri, sizeof(ctx->redirect_uri));

    return true;
}

const char*
fw_get_auth_url(funkctx *ctx)
{
    if (!*ctx->auth_url) {
        snprintf(ctx->auth_url, sizeof(ctx->auth_url),
                 "%s/authorize?response_type=code&redirect_uri=%s&clint_id=%s&scope=%s",
                 ctx->url,
                 url_encode(ctx->redirect_uri, (char[3*sizeof(ctx->redirect_uri)]){'\0'}),
                 url_encode(ctx->client_id,    (char[3*sizeof(ctx->client_id)]){'\0'}),
                 url_encode(ctx->scope,        (char[3*sizeof(ctx->scope)]){'\0'}));
    }

    return ctx->auth_url;
}

bool
fw_set_user_token(funkctx *ctx, const char *token)
{
    strncpy(ctx->user_token, token, sizeof(ctx->user_token));

    return true;
}

int
main(void)
{
    url_enc_init();
#if 1
    funkctx *ctx = fw_init(SCHEME, SERVER);
#else
    funkctx *ctx = fw_init("https", "funkwhale.it");
#endif

    if (!ctx) {
        fprintf(stderr, "ERR: Couldn't initialize a funkwhale context\n");
        return 1;
    }

    fw_set_app_token(ctx, APP_ID, APP_SECRET, "read write:libraries", "urn:ietf:wg:oauth:2.0:oob");
    fw_set_user_token(ctx, USER_TOKEN);

    /*
    fw_channel channel = {
        .name = "Test channel",
        .username = "testchannel",
        .descx = "Description of the channel",
    };

    if (!fw_create_channel(ctx, &channel))
        printf("An error occured\n");

    if (!fw_get(ctx, FW_CHANNELS, NULL))
        printf("An error occured while getting channels\n");

    print_results(ctx);
    */

    fw_attach(ctx, fopen("./cover.jpg", "r"), "image/jpeg");
    print_results(ctx);

    return 0;
}
