#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESULT_SIZE 131072
#define TRANSFER_SIZE 32768

static char result[RESULT_SIZE];
static size_t result_length;
static int event_count;

static void append(const char *format, ...)
{
    va_list args;
    int written;

    if (result_length >= sizeof(result))
        return;
    va_start(args, format);
    written = vsnprintf(result + result_length, sizeof(result) - result_length,
        format, args);
    va_end(args);
    if (written > 0)
        result_length += (size_t)written < sizeof(result) - result_length
            ? (size_t)written : sizeof(result) - result_length - 1;
}

static void append_json_string(const char *value)
{
    const unsigned char *p = (const unsigned char *)value;
    append("\"");
    while (*p != '\0') {
        if (*p == '\\' || *p == '"') append("\\%c", *p);
        else if (*p == '\n') append("\\n");
        else if (*p >= 0x20) append("%c", *p);
        p++;
    }
    append("\"");
}

static const char *record_name(unsigned int type)
{
    switch (type) {
    case 20: return "ChangeCipherSpec";
    case 21: return "Alert";
    case 22: return "Handshake";
    case 23: return "ApplicationData";
    default: return "Unknown";
    }
}

static const char *handshake_name(unsigned int type)
{
    switch (type) {
    case 0: return "HelloRequest";
    case 1: return "ClientHello";
    case 2: return "ServerHello";
    case 11: return "Certificate";
    case 12: return "ServerKeyExchange";
    case 13: return "CertificateRequest";
    case 14: return "ServerHelloDone";
    case 15: return "CertificateVerify";
    case 16: return "ClientKeyExchange";
    case 20: return "Finished";
    default: return "EncryptedHandshake";
    }
}

static void add_event(const char *direction, const char *label,
    const char *record, size_t bytes, const char *state)
{
    if (event_count++ != 0)
        append(",");
    append("{\"direction\":"); append_json_string(direction);
    append(",\"label\":"); append_json_string(label);
    append(",\"record\":"); append_json_string(record);
    append(",\"bytes\":%zu,\"state\":", bytes);
    append_json_string(state);
    append("}");
}

static int transfer(SSL *sender, SSL *receiver, const char *direction)
{
    BIO *out = SSL_get_wbio(sender);
    BIO *in = SSL_get_rbio(receiver);
    unsigned char buffer[TRANSFER_SIZE];
    int total = 0;
    int length;

    while ((length = BIO_read(out, buffer, sizeof(buffer))) > 0) {
        size_t offset = 0;
        total += length;
        if (BIO_write(in, buffer, length) != length)
            return -1;

        while (offset + 5 <= (size_t)length) {
            unsigned int type = buffer[offset];
            size_t record_length = ((size_t)buffer[offset + 3] << 8) |
                buffer[offset + 4];
            const char *label = record_name(type);
            size_t whole = record_length + 5;

            if (offset + whole > (size_t)length)
                whole = (size_t)length - offset;
            if (type == 22 && record_length >= 4 && offset + 6 <= (size_t)length)
                label = handshake_name(buffer[offset + 5]);
            add_event(direction, label, record_name(type), whole,
                SSL_state_string_long(sender));
            offset += whole;
        }
    }
    return total;
}

static X509 *read_certificate(const unsigned char *data, size_t length)
{
    BIO *bio = BIO_new_mem_buf(data, (int)length);
    X509 *certificate = bio != NULL ? PEM_read_bio_X509(bio, NULL, NULL, NULL) : NULL;
    BIO_free(bio);
    return certificate;
}

static EVP_PKEY *read_private_key(const unsigned char *data, size_t length)
{
    BIO *bio = BIO_new_mem_buf(data, (int)length);
    EVP_PKEY *key = bio != NULL ? PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL) : NULL;
    BIO_free(bio);
    return key;
}

static void fail(const char *message)
{
    result_length = 0;
    append("{\"ok\":false,\"error\":");
    append_json_string(message);
    append("}");
}

void *tls_lab_alloc(size_t size) { return malloc(size); }
void tls_lab_free(void *pointer) { free(pointer); }
const char *tls_lab_result(void) { return result; }

int tls_lab_run(const unsigned char *certificate_data, size_t certificate_length,
    const unsigned char *key_data, size_t key_length)
{
    SSL_CTX *client_context = NULL;
    SSL_CTX *server_context = NULL;
    SSL *client = NULL;
    SSL *server = NULL;
    X509 *certificate = NULL;
    EVP_PKEY *key = NULL;
    BIO *client_read = NULL, *client_write = NULL;
    BIO *server_read = NULL, *server_write = NULL;
    char application_buffer[16] = {0};
    int success = 0;

    SSL_library_init();
    certificate = read_certificate(certificate_data, certificate_length);
    key = read_private_key(key_data, key_length);
    if (certificate == NULL || key == NULL) {
        fail("Could not parse the demo certificate or private key.");
        goto done;
    }

    client_context = SSL_CTX_new(TLS_method());
    server_context = SSL_CTX_new(TLS_method());
    if (client_context == NULL || server_context == NULL ||
        SSL_CTX_set_min_proto_version(client_context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(client_context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_min_proto_version(server_context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(server_context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate(server_context, certificate) != 1 ||
        SSL_CTX_use_PrivateKey(server_context, key) != 1) {
        fail("Could not configure the TLS endpoints.");
        goto done;
    }
    SSL_CTX_set_verify(client_context, SSL_VERIFY_NONE, NULL);

    client = SSL_new(client_context);
    server = SSL_new(server_context);
    client_read = BIO_new(BIO_s_mem());
    client_write = BIO_new(BIO_s_mem());
    server_read = BIO_new(BIO_s_mem());
    server_write = BIO_new(BIO_s_mem());
    if (client == NULL || server == NULL || client_read == NULL ||
        client_write == NULL || server_read == NULL || server_write == NULL) {
        fail("Could not create in-memory TLS endpoints.");
        goto done;
    }

    SSL_set_bio(client, client_read, client_write);
    client_read = client_write = NULL;
    SSL_set_bio(server, server_read, server_write);
    server_read = server_write = NULL;
    SSL_set_connect_state(client);
    SSL_set_accept_state(server);
    SSL_set_tlsext_host_name(client, "lab.example");

    result_length = 0;
    event_count = 0;
    append("{\"ok\":true,\"events\":[");
    for (int round = 0; round < 32; round++) {
        int client_result = 1;
        int server_result = 1;

        if (!SSL_is_init_finished(client)) {
            client_result = SSL_do_handshake(client);
            if (client_result != 1 && SSL_get_error(client, client_result) != SSL_ERROR_WANT_READ) {
                fail("The TLS client handshake failed.");
                goto done;
            }
        }
        if (transfer(client, server, "client-to-server") < 0) {
            fail("Could not transfer client TLS records.");
            goto done;
        }
        if (!SSL_is_init_finished(server)) {
            server_result = SSL_do_handshake(server);
            if (server_result != 1 && SSL_get_error(server, server_result) != SSL_ERROR_WANT_READ) {
                fail("The TLS server handshake failed.");
                goto done;
            }
        }
        if (transfer(server, client, "server-to-client") < 0) {
            fail("Could not transfer server TLS records.");
            goto done;
        }
        if (SSL_is_init_finished(client) && SSL_is_init_finished(server))
            break;
    }
    if (!SSL_is_init_finished(client) || !SSL_is_init_finished(server)) {
        fail("The TLS handshake did not complete.");
        goto done;
    }

    if (SSL_write(client, "ping", 4) != 4 ||
        transfer(client, server, "client-to-server") < 0 ||
        SSL_read(server, application_buffer, sizeof(application_buffer)) != 4) {
        fail("Could not exchange encrypted application data.");
        goto done;
    }
    if (SSL_write(server, "pong", 4) != 4 ||
        transfer(server, client, "server-to-client") < 0 ||
        SSL_read(client, application_buffer, sizeof(application_buffer)) != 4) {
        fail("Could not exchange the server response.");
        goto done;
    }

    append("],\"version\":"); append_json_string(SSL_get_version(client));
    append(",\"cipher\":"); append_json_string(SSL_get_cipher(client));
    append(",\"applicationData\":\"ping → pong\"}");
    success = 1;

done:
    BIO_free(client_read); BIO_free(client_write);
    BIO_free(server_read); BIO_free(server_write);
    SSL_free(client); SSL_free(server);
    SSL_CTX_free(client_context); SSL_CTX_free(server_context);
    EVP_PKEY_free(key); X509_free(certificate);
    return success;
}
