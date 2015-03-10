/*
  HTTPRequest.h - Library for processing HTTP requests.
  Created by Nicholas Pitt, February 26, 2015.
  Released under the LGPL license.
*/

#ifndef HTTPRequest_h
#define HTTPRequest_h

#include "Arduino.h"
#include "Client.h"
#include "SD.h"

enum Method {
  GET,
  POST,
  HEAD
};

template <size_t T, size_t U, size_t V, size_t W>
class HTTPRequest {
public:
  HTTPRequest() {
    header_ = new char[T];
    uri_ = new char[U];
    params_ = new char[V];
    cookies_ = new char[W];
  }
  ~HTTPRequest() {
    delete[] header_;
    delete[] uri_;
    delete[] params_;
    delete[] cookies_;
  }
  Method method() { return method_; }
  char *uri() const { return uri_; }
  char *params() const { return params_; }
  char *cookies() const { return cookies_; }
  bool getParam(char *name, char *header_, int size) {
    return parse(params_, '&', name, header_, size); }
  bool getCookie(char *name, char *header_, int size) {
    return parse(cookies_, ';', name, header_, size); }
  bool begin(Client &client) {
    bool method_header = true, success = true;
    size_t index = 0, length, content_length = 0;
    char method_string[5], *start, *delim, *end;

    client_ = &client;
    params_[0] = cookies_[0] = '\0';
    while (client_->connected()) {
      if (client_->available()) {
        if (index == T) {
          method_header = success = false;
          index = 0;
        }
        header_[index++] = client_->read();
        if (header_[index - 1] == '\n') {
          header_[index - min(index, 2)] = '\0';
          index = 0;
          if (method_header) {
            end = strchr(header_, ' ');
            if (!end) {
              method_header = success = false;
              continue;
            }
            length = min(end - header_ + 1, 5);
            snprintf(method_string, length, "%s", header_);
            if (!strcmp(method_string, "GET")) {
              method_ = GET;
            } else if (!strcmp(method_string, "POST")) {
              method_ = POST;
            } else if (!strcmp(method_string, "HEAD")) {
              method_ = HEAD;
            } else {
              method_header = success = false;
            }
            start = end + 1;
            end = strchr(start, ' ');
            if (!end) {
              method_header = success = false;
              continue;
            }
            delim = strchr(start, '?');
            if (delim) {
              length = end - delim;
              snprintf(params_, length, "%s", delim + 1);
              length = delim - start + 1;
            } else {
              length = end - start + 1;
            }
            snprintf(uri_, length, "%s", start);
            method_header = false;
          } else if (method_ == POST && strstr(header_, "Content-Length: ") == header_) {
            content_length = strtol(header_ + 16, NULL, 10);
          } else if (strstr(header_, "Cookie: ") == header_) {
            start = header_ + 8;
            length = strlen(start) + 1;
            cookies_ = new char[length];
            snprintf(cookies_, length, "%s", start);
            urlDecode(cookies_);
          } else if (!strlen(header_)) {
            if (method_ == POST && content_length) {
              index = 0;
              while (client_->connected()) {
                if (client_->available()) {
                  if (index == V) {
                    success = false;
                    break;
                  }
                  params_[index++] = client_->read();
                  content_length--;
                  if (!content_length) {
                    params_[index] = '\0';
                    break;
                  }
                }
              }
            }
            break;
          }
        }
      }
    }
    return success;
  }
  void stringResponse(char *response, char *content_type, char *content,
                      char *cookie_name = NULL, char *cookie_value = NULL,
                      char *cookie_path = NULL) {
    if (client_->connected()) {    
      sendHeaders(response, content ? strlen(content) : 0, content_type, cookie_name,
                  cookie_value, cookie_path);
      if (method_ != HEAD && content) {
        client_->print(content);
      }
    }
  }
  void fileResponse(char *response, char *content_type, File &content,
                    char *cookie_name = NULL, char *cookie_value = NULL,
                    char *cookie_path = NULL) {
    if (client_->connected()) {
      sendHeaders(response, content ? content.size() : 0, content_type, cookie_name,
                  cookie_value, cookie_path);
      if (method_ != HEAD && content) {
        size_t i, size = 2048;
        byte *buffer;

        buffer = new byte[size];
        while(!buffer) {
          buffer = new byte[--size];
        }
        content.seek(0);
        while (content.available()) {
          for (i = 0; i < size && content.available(); i++) {
            buffer[i] = content.read();
          }
          client_->write(buffer, i);
        }
        delete[] buffer;
      }
    }
  }

private:
  Client *client_;
  Method method_;
  char *header_;
  char *uri_;
  char *params_;
  char *cookies_;

  bool parse(char *data, char delim, char *name, char *buffer, int size) const {
    int new_size = size, token_size = strlen(name) + 2;
    char token[token_size], *start, *end;

    snprintf(token, token_size, "%s=", name);
    start = strstr(data, token);
    if (start) {
      start += strlen(token);
      end = strchr(start, delim);
      if (end) {
        new_size = min(end - start + 1, size);
      }
      snprintf(buffer, new_size, "%s", start);
      urlDecode(buffer);
      return true;
    }
    return false;
  }
  void urlDecode(char *string) const {
    size_t length = strlen(string) + 1;
    char buffer[length], *percent, *last = string;

    buffer[0] = '\0';
    percent = strchr(last, '%');
    while (percent) {
      char token[3];

      snprintf(token, 3, "%s", percent + 1);
      if (strlen(token) == 2) {
        char symbol = (char)strtol(token, NULL, 16);

        strncat(buffer, last, percent - last);
        strncat(buffer, &symbol, 1);
        percent += 2;
      }
      last = percent + 1;
      percent = strchr(last, '%');
    }
    if (strlen(buffer)) {
      strcat(buffer, last);
      snprintf(string, length, "%s", buffer);
    }
  }
  void sendHeaders(char *response, size_t content_length, char *content_type,
                   char *cookie_name, char *cookie_value, char *cookie_path) const {
    client_->print("HTTP/1.1 ");
    client_->println(response);
    client_->print("Connection: close\r\nContent-Length: ");
    client_->println(content_length);
    if (content_type) {
      client_->print("Content-Type: ");
      client_->println(content_type);
    }
    if (cookie_name && cookie_value && cookie_path) {
      client_->print("Set-Cookie: ");
      client_->print(cookie_name);
      client_->print("=");
      client_->print(cookie_value);
      client_->print("; path=");
      client_->println(cookie_path);
    }
    client_->println();
  }
};

#endif