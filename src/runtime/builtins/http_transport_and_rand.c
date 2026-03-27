    const char *p = rest;
    while (*p != '\0' && *p != '/' && *p != '?' && *p != '#') {
        p++;
    }
    size_t hostport_len = (size_t)(p - rest);
    if (hostport_len == 0) {
        snprintf(err, err_cap, "%s url missing host", scheme);
        return 0;
    }

    char *hostport = xstrndup(rest, hostport_len);
    char *colon = strrchr(hostport, ':');
    char *host = NULL;
    int port = default_port;
    if (colon) {
        *colon = '\0';
        if (hostport[0] == '\0') {
            free(hostport);
            snprintf(err, err_cap, "%s url missing host", scheme);
            return 0;
        }
        const char *port_s = colon + 1;
        if (*port_s == '\0') {
            free(hostport);
            snprintf(err, err_cap, "%s url has invalid port", scheme);
            return 0;
        }
        char *end = NULL;
        long parsed = strtol(port_s, &end, 10);
        if (end == NULL || *end != '\0' || parsed <= 0 || parsed > 65535) {
            free(hostport);
            snprintf(err, err_cap, "%s url has invalid port", scheme);
            return 0;
        }
        port = (int)parsed;
    }
    host = xstrdup(hostport);
    free(hostport);

    char *path = NULL;
    if (*p == '\0') {
        path = xstrdup("/");
    } else {
        ReTextBuf pb;
        if (!re_text_buf_init(&pb)) {
            free(host);
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
        if (*p == '?' || *p == '#') {
            if (!re_text_buf_push_cstr(&pb, "/")) {
                free(pb.buf);
                free(host);
                snprintf(err, err_cap, "out of memory");
                return 0;
            }
        }
        const char *q = p;
        while (*q != '\0' && *q != '#') {
            if (!re_text_buf_push_n(&pb, q, 1)) {
                free(pb.buf);
                free(host);
                snprintf(err, err_cap, "out of memory");
                return 0;
            }
            q++;
        }
        path = pb.buf;
    }

    *out_host = host;
    *out_port = port;
    *out_path = path;
    return 1;
}

static int http_connect_tcp(const char *host, int port, char *err, size_t err_cap) {
    if (port <= 0 || port > 65535) {
        snprintf(err, err_cap, "http url has invalid port");
        return -1;
    }

    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
        snprintf(err, err_cap, "http connect failed");
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        snprintf(err, err_cap, "http connect failed");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        snprintf(err, err_cap, "http connect failed");
        return -1;
    }
    return fd;
}

static int http_build_request(const char *method, const char *host, int port, int is_https, const char *path,
                              const char *body, int has_body, ReTextBuf *out_req, char *err, size_t err_cap) {
    ReTextBuf req;
    char host_port[16];
    int add_port = (is_https && port != 443) || (!is_https && port != 80);
    if (!re_text_buf_init(&req)) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }

    if (add_port) {
        snprintf(host_port, sizeof(host_port), "%d", port);
    }

    if (!re_text_buf_push_cstr(&req, method) || !re_text_buf_push_cstr(&req, " ") ||
        !re_text_buf_push_cstr(&req, path) || !re_text_buf_push_cstr(&req, " HTTP/1.0\r\nHost: ") ||
        !re_text_buf_push_cstr(&req, host) || (add_port && !re_text_buf_push_cstr(&req, ":")) ||
        (add_port && !re_text_buf_push_cstr(&req, host_port)) ||
        !re_text_buf_push_cstr(&req, "\r\nUser-Agent: ajasendiri/0.1\r\nConnection: close\r\n")) {
        free(req.buf);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }

    if (has_body) {
        char len_buf[64];
        size_t body_len = strlen(body);
        snprintf(len_buf, sizeof(len_buf), "%zu", body_len);
        if (!re_text_buf_push_cstr(&req, "Content-Type: text/plain\r\nContent-Length: ") ||
            !re_text_buf_push_cstr(&req, len_buf) || !re_text_buf_push_cstr(&req, "\r\n")) {
            free(req.buf);
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
    }
    if (!re_text_buf_push_cstr(&req, "\r\n")) {
        free(req.buf);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    if (has_body && !re_text_buf_push_cstr(&req, body)) {
        free(req.buf);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    *out_req = req;
    return 1;
}

static int http_read_response_fd(int fd, ReTextBuf *resp, char *err, size_t err_cap) {
    ReTextBuf out;
    if (!re_text_buf_init(&out)) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    char chunk[1024];
    while (1) {
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0) {
            free(out.buf);
            snprintf(err, err_cap, "http read failed");
            return 0;
        }
        if (n == 0) {
            break;
        }
        if (!re_text_buf_push_n(&out, chunk, (size_t)n)) {
            free(out.buf);
            snprintf(err, err_cap, "out of memory");
            return 0;
        }
    }
    *resp = out;
    return 1;
}

static int http_read_response_ssl(SSL *ssl, ReTextBuf *out_resp, char *err, size_t err_cap) {
    ReTextBuf resp;
    if (!re_text_buf_init(&resp)) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }

    char chunk[2048];
    while (1) {
        int n = SSL_read(ssl, chunk, (int)sizeof(chunk));
        if (n > 0) {
            if (!re_text_buf_push_n(&resp, chunk, (size_t)n)) {
                free(resp.buf);
                snprintf(err, err_cap, "out of memory");
                return 0;
            }
            continue;
        }
        int ssl_err = SSL_get_error(ssl, n);
        if (ssl_err == SSL_ERROR_ZERO_RETURN) {
            break;
        }
        if (ssl_err == SSL_ERROR_WANT_READ || ssl_err == SSL_ERROR_WANT_WRITE) {
            continue;
        }
        free(resp.buf);
        snprintf(err, err_cap, "http read failed");
        return 0;
    }
    *out_resp = resp;
    return 1;
}

static int http_request_over_tcp(const char *method, const char *host, int port, const char *path, const char *body,
                                 int has_body, int require_2xx, int with_headers, HttpResponseParsed *out, char *err,
                                 size_t err_cap) {
    int fd = http_connect_tcp(host, port, err, err_cap);
    if (fd < 0) {
        return 0;
    }

    ReTextBuf req;
    if (!http_build_request(method, host, port, 0, path, body, has_body, &req, err, err_cap)) {
        close(fd);
        return 0;
    }

    if (!http_write_all_fd(fd, req.buf, req.len)) {
        free(req.buf);
        close(fd);
        snprintf(err, err_cap, "http write failed");
        return 0;
    }
    free(req.buf);

    ReTextBuf resp;
    if (!http_read_response_fd(fd, &resp, err, err_cap)) {
        close(fd);
        return 0;
    }
    close(fd);

    int ok = http_parse_response(method, resp.buf, resp.len, require_2xx, with_headers, out, err, err_cap);
    free(resp.buf);
    return ok;
}

static int http_request_over_tls(const char *method, const char *host, int port, const char *path, const char *body,
                                 int has_body, int require_2xx, int with_headers, HttpResponseParsed *out, char *err,
                                 size_t err_cap) {
    int fd = http_connect_tcp(host, port, err, err_cap);
    if (fd < 0) {
        return 0;
    }

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        close(fd);
        snprintf(err, err_cap, "https tls init failed");
        return 0;
    }
    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https tls init failed");
        return 0;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https tls init failed");
        return 0;
    }
    if (SSL_set_tlsext_host_name(ssl, host) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https tls init failed");
        return 0;
    }
    if (SSL_set_fd(ssl, fd) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https tls init failed");
        return 0;
    }
    if (SSL_connect(ssl) != 1) {
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https tls handshake failed");
        return 0;
    }
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "https certificate verify failed");
        return 0;
    }

    ReTextBuf req;
    if (!http_build_request(method, host, port, 1, path, body, has_body, &req, err, err_cap)) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return 0;
    }
    if (!http_write_all_ssl(ssl, req.buf, req.len)) {
        free(req.buf);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        snprintf(err, err_cap, "http write failed");
        return 0;
    }
    free(req.buf);

    ReTextBuf resp;
    if (!http_read_response_ssl(ssl, &resp, err, err_cap)) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(fd);
        return 0;
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(fd);

    int ok = http_parse_response(method, resp.buf, resp.len, require_2xx, with_headers, out, err, err_cap);
    free(resp.buf);
    return ok;
}

static int http_response_set_file_headers(MapValue *headers, const char *body) {
    char len_buf[64];
    snprintf(len_buf, sizeof(len_buf), "%zu", strlen(body));
    if (!map_set(headers, "content-type", value_string("text/plain")) ||
        !map_set(headers, "content-length", value_string(len_buf))) {
        return 0;
    }
    return 1;
}

static int http_request_exec(const char *method, const char *url, const char *body, int has_body, int require_2xx,
                             int with_headers, HttpResponseParsed *out, char *err, size_t err_cap) {
    http_response_parsed_init(out);
    if (!http_url_is_safe(url)) {
        snprintf(err, err_cap, "http url contains unsupported characters");
        return 0;
    }
    if (!http_method_is_safe(method)) {
        snprintf(err, err_cap, "http method must be uppercase letters");
        return 0;
    }

    if (strncmp(url, "file://", strlen("file://")) == 0) {
        char *path = http_file_url_to_path(url, err, err_cap);
        if (!path) {
            return 0;
        }
        char *txt = fs_read_text_file(path, err, err_cap);
        free(path);
        if (!txt) {
            return 0;
        }
        out->status = 200;
        out->body = txt;
        if (with_headers) {
            out->headers = map_new(VT_STRING, NULL);
            if (!out->headers || !http_response_set_file_headers(out->headers, txt)) {
                http_response_parsed_cleanup(out);
                snprintf(err, err_cap, "out of memory");
                return 0;
            }
        }
        return 1;
    }

    if (strncmp(url, "http://", strlen("http://")) == 0) {
        char *host = NULL;
        char *path = NULL;
        int port = 80;
        int ok = 0;
        if (!http_parse_network_url(url, "http", 80, &host, &port, &path, err, err_cap)) {
            return 0;
        }
        ok = http_request_over_tcp(method, host, port, path, body, has_body, require_2xx, with_headers, out, err, err_cap);
        free(host);
        free(path);
        return ok;
    }

    if (strncmp(url, "https://", strlen("https://")) == 0) {
        char *host = NULL;
        char *path = NULL;
        int port = 443;
        int ok = 0;
        if (!http_parse_network_url(url, "https", 443, &host, &port, &path, err, err_cap)) {
            return 0;
        }
        ok = http_request_over_tls(method, host, port, path, body, has_body, require_2xx, with_headers, out, err, err_cap);
        free(host);
        free(path);
        return ok;
    }

    snprintf(err, err_cap, "http url must start with http://, https://, or file://");
    return 0;
}

static TypeRef http_simple_type_ref(ValueType kind) {
    TypeRef t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    return t;
}

static Value http_make_response_object(Runtime *rt, int line, HttpResponseParsed *resp) {
    MapValue *headers = resp->headers;
    if (!headers) {
        headers = map_new(VT_STRING, NULL);
        if (!headers) {
            runtime_error(rt, line, "out of memory");
            return value_invalid();
        }
    }

    ObjectValue *obj = (ObjectValue *)calloc(1, sizeof(ObjectValue));
    if (!obj) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }
    obj->type_name = xstrdup("HttpResponse");
    obj->field_count = 3;
    obj->fields = (ObjectFieldDef *)calloc(3, sizeof(ObjectFieldDef));
    obj->values = (Value *)calloc(3, sizeof(Value));
    if (!obj->type_name || !obj->fields || !obj->values) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    obj->fields[0].name = xstrdup("status");
    obj->fields[0].type = http_simple_type_ref(VT_INT);
    obj->values[0] = value_int(resp->status);

    obj->fields[1].name = xstrdup("headers");
    obj->fields[1].type = http_simple_type_ref(VT_MAP);
    obj->values[1] = value_map(headers);

    obj->fields[2].name = xstrdup("body");
    obj->fields[2].type = http_simple_type_ref(VT_STRING);
    obj->values[2].type = VT_STRING;
    obj->values[2].as.s = resp->body ? resp->body : xstrdup("");

    if (!obj->fields[0].name || !obj->fields[1].name || !obj->fields[2].name || !obj->values[2].as.s) {
        runtime_error(rt, line, "out of memory");
        return value_invalid();
    }

    resp->headers = NULL;
    resp->body = NULL;
    return value_object(obj);
}

static char *http_request_text(const char *method, const char *url, const char *body, int has_body, char *err,
                               size_t err_cap) {
    HttpResponseParsed parsed;
    http_response_parsed_init(&parsed);
    if (!http_request_exec(method, url, body, has_body, 1, 0, &parsed, err, err_cap)) {
        http_response_parsed_cleanup(&parsed);
        return NULL;
    }
    char *txt = parsed.body;
    parsed.body = NULL;
    http_response_parsed_cleanup(&parsed);
    return txt;
}

static unsigned long long rand_next_u64(Runtime *rt) {
    if (!rt->rand_seeded) {
        unsigned long long seed = 0x9e3779b97f4a7c15ULL;
        if (!rt->check_only) {
            seed ^= (unsigned long long)time(NULL);
        }
        rt->rand_state = seed;
        rt->rand_seeded = 1;
    }
    unsigned long long x = rt->rand_state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    rt->rand_state = x;
    return x * 2685821657736338717ULL;
}

