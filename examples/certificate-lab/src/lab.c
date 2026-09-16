#include <arpa/inet.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESULT_SIZE 32768

static char result[RESULT_SIZE];
static size_t result_length;

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
        switch (*p) {
        case '\\': append("\\\\"); break;
        case '"': append("\\\""); break;
        case '\n': append("\\n"); break;
        case '\r': append("\\r"); break;
        case '\t': append("\\t"); break;
        default:
            if (*p < 0x20)
                append("\\u%04x", *p);
            else
                append("%c", *p);
        }
        p++;
    }
    append("\"");
}

static X509 *read_certificate(const unsigned char *data, size_t length)
{
    BIO *bio;
    X509 *certificate;

    bio = BIO_new_mem_buf(data, (int)length);
    if (bio == NULL)
        return NULL;
    certificate = PEM_read_bio_X509(bio, NULL, NULL, NULL);
    BIO_free(bio);
    return certificate;
}

static void append_name(const char *key, X509_NAME *name)
{
    char *text = X509_NAME_oneline(name, NULL, 0);

    append("\""); append(key); append("\":");
    append_json_string(text != NULL ? text : "Unknown");
    OPENSSL_free(text);
}

static void append_time(const char *key, const ASN1_TIME *time)
{
    BIO *bio = BIO_new(BIO_s_mem());
    char *data = NULL;
    long length = 0;

    if (bio != NULL && ASN1_TIME_print(bio, time) == 1)
        length = BIO_get_mem_data(bio, &data);
    append("\""); append(key); append("\":");
    if (length > 0) {
        char text[128];
        size_t copy = (size_t)length < sizeof(text) - 1 ? (size_t)length : sizeof(text) - 1;
        memcpy(text, data, copy);
        text[copy] = '\0';
        append_json_string(text);
    } else {
        append_json_string("Unknown");
    }
    BIO_free(bio);
}

static void append_fingerprint(X509 *certificate)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    append("\"fingerprint\":\"");
    if (X509_digest(certificate, EVP_sha256(), digest, &length) == 1) {
        for (unsigned int i = 0; i < length; i++) {
            if (i != 0)
                append(":");
            append("%02X", digest[i]);
        }
    }
    append("\"");
}

static void append_sans(X509 *certificate)
{
    GENERAL_NAMES *names = X509_get_ext_d2i(certificate,
        NID_subject_alt_name, NULL, NULL);

    append("\"sans\":[");
    if (names != NULL) {
        int count = sk_GENERAL_NAME_num(names);
        int emitted = 0;
        for (int i = 0; i < count; i++) {
            GENERAL_NAME *name = sk_GENERAL_NAME_value(names, i);
            char text[INET6_ADDRSTRLEN];
            const char *value = NULL;

            if (name->type == GEN_DNS) {
                value = (const char *)ASN1_STRING_get0_data(name->d.dNSName);
            } else if (name->type == GEN_IPADD) {
                const unsigned char *bytes = ASN1_STRING_get0_data(name->d.iPAddress);
                int length = ASN1_STRING_length(name->d.iPAddress);
                if (length == 4)
                    value = inet_ntop(AF_INET, bytes, text, sizeof(text));
                else if (length == 16)
                    value = inet_ntop(AF_INET6, bytes, text, sizeof(text));
            }
            if (value != NULL) {
                if (emitted++) append(",");
                append_json_string(value);
            }
        }
        GENERAL_NAMES_free(names);
    }
    append("]");
}

static void set_error(const char *message)
{
    result_length = 0;
    append("{\"ok\":false,\"error\":");
    append_json_string(message);
    append("}");
}

void *lab_alloc(size_t size)
{
    return malloc(size);
}

void lab_free(void *pointer)
{
    free(pointer);
}

const char *lab_result(void)
{
    return result;
}

int lab_parse_certificate(const unsigned char *data, size_t length)
{
    X509 *certificate = read_certificate(data, length);
    EVP_PKEY *public_key;
    int signature_nid;

    if (certificate == NULL) {
        set_error("Could not parse a PEM certificate.");
        return 0;
    }

    result_length = 0;
    append("{\"ok\":true,");
    append_name("subject", X509_get_subject_name(certificate)); append(",");
    append_name("issuer", X509_get_issuer_name(certificate)); append(",");
    append_time("notBefore", X509_get0_notBefore(certificate)); append(",");
    append_time("notAfter", X509_get0_notAfter(certificate)); append(",");

    public_key = X509_get_pubkey(certificate);
    append("\"publicKey\":{");
    if (public_key != NULL) {
        int key_nid = EVP_PKEY_id(public_key);
        append("\"algorithm\":");
        append_json_string(OBJ_nid2ln(key_nid));
        append(",\"bits\":%d", EVP_PKEY_bits(public_key));
    } else {
        append("\"algorithm\":\"Unknown\",\"bits\":0");
    }
    append("},");
    EVP_PKEY_free(public_key);

    signature_nid = X509_get_signature_nid(certificate);
    append("\"signatureAlgorithm\":");
    append_json_string(OBJ_nid2ln(signature_nid)); append(",");
    append_sans(certificate); append(",");
    append_fingerprint(certificate);
    append("}");

    X509_free(certificate);
    return 1;
}

int lab_verify_certificate(const unsigned char *certificate_data,
    size_t certificate_length, const unsigned char *ca_data, size_t ca_length)
{
    X509 *certificate = read_certificate(certificate_data, certificate_length);
    X509 *ca = read_certificate(ca_data, ca_length);
    X509_STORE *store = NULL;
    X509_STORE_CTX *context = NULL;
    int verified = 0;
    int error = X509_V_ERR_UNSPECIFIED;

    if (certificate == NULL || ca == NULL) {
        set_error("Could not parse the certificate or CA certificate.");
        goto done;
    }
    store = X509_STORE_new();
    context = X509_STORE_CTX_new();
    if (store == NULL || context == NULL ||
        X509_STORE_add_cert(store, ca) != 1 ||
        X509_STORE_CTX_init(context, store, certificate, NULL) != 1) {
        set_error("Could not initialize certificate verification.");
        goto done;
    }

    verified = X509_verify_cert(context) == 1;
    error = X509_STORE_CTX_get_error(context);
    result_length = 0;
    append("{\"ok\":true,\"verified\":%s,\"message\":",
        verified ? "true" : "false");
    append_json_string(verified ? "Certificate verified successfully."
        : X509_verify_cert_error_string(error));
    append(",\"errorCode\":%d}", error);

done:
    X509_STORE_CTX_free(context);
    X509_STORE_free(store);
    X509_free(ca);
    X509_free(certificate);
    return verified;
}
