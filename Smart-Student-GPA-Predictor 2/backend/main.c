
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <windows.h>

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

#define PORT 8081
#define MAX_SUB 10
#define MAX_SEM 8
#define BUFFER_SIZE 32768

typedef struct {
    char name[100];
    int credits;
    float internal;
    float external;
    float total;
    char grade[3];
    int point;
} Subject;

Subject subjects[MAX_SUB];
int subjectCount = 0;

float semesterGPA = 0;
float updatedCGPA = 0;
float previousGPA[MAX_SEM];
int previousCount = 0;

char requestBuffer[BUFFER_SIZE];
char responseBuffer[BUFFER_SIZE * 2];

// ---------------- GRADE CALCULATION ----------------

int gradePoint(float marks, char *grade) {

    if (marks >= 90) {
        strcpy(grade, "S");
        return 10;
    }
    else if (marks >= 85) {
        strcpy(grade, "A+");
        return 9;
    }
    else if (marks >= 80) {
        strcpy(grade, "A");
        return 8;
    }
    else if (marks >= 70) {
        strcpy(grade, "B+");
        return 7;
    }
    else if (marks >= 60) {
        strcpy(grade, "B");
        return 6;
    }
    else if (marks >= 50) {
        strcpy(grade, "C");
        return 5;
    }
    else if (marks >= 40) {
        strcpy(grade, "P");
        return 4;
    }

    strcpy(grade, "F");
    return 0;
}

// ---------------- SEMESTER GPA ----------------

float calculateGPA() {

    float totalPoints = 0;
    int totalCredits = 0;

    for (int i = 0; i < subjectCount; i++) {

        subjects[i].total =
            subjects[i].internal +
            subjects[i].external * 0.5f;

        subjects[i].point =
            gradePoint(subjects[i].total,
                       subjects[i].grade);

        totalPoints +=
            subjects[i].credits * subjects[i].point;

        totalCredits += subjects[i].credits;
    }

    return totalCredits > 0
        ? totalPoints / totalCredits
        : 0;
}

// ---------------- CGPA ----------------

float calculateCGPA() {

    float total = semesterGPA;

    for (int i = 0; i < previousCount; i++) {
        total += previousGPA[i];
    }

    return total / (previousCount + 1);
}

// ---------------- GPA PREDICTION ----------------

float predictNext() {

    int n = previousCount + 1;

    float gpas[MAX_SEM];

    for (int i = 0; i < previousCount; i++) {
        gpas[i] = previousGPA[i];
    }

    gpas[n - 1] = semesterGPA;

    if (n == 1) {
        return gpas[0];
    }

    float sx = 0, sy = 0;
    float sxy = 0, sxx = 0;

    for (int i = 0; i < n; i++) {

        float x = i + 1;

        sx += x;
        sy += gpas[i];
        sxy += x * gpas[i];
        sxx += x * x;
    }

    float denominator = n * sxx - sx * sx;

    if (denominator == 0) {
        return gpas[n - 1];
    }

    float b = (n * sxy - sx * sy) / denominator;

    float a = (sy - b * sx) / n;

    float prediction = a + b * (n + 1);

    if (prediction > 10) prediction = 10;
    if (prediction < 0) prediction = 0;

    return prediction;
}

// ---------------- SAVE RECORDS ----------------

void saveRecords() {

    FILE *fp = fopen("backend/records.txt", "a");

    if (fp == NULL) {
        printf("Unable to open records.txt\n");
        return;
    }

    fprintf(fp, "\nSMART STUDENT GPA REPORT\n");
    fprintf(fp, "Semester GPA: %.2f\n", semesterGPA);
    fprintf(fp, "Updated CGPA: %.2f\n", updatedCGPA);

    for (int i = 0; i < subjectCount; i++) {

        fprintf(fp,
            "%s | Credits: %d | Total: %.1f | Grade: %s | GP: %d\n",
            subjects[i].name,
            subjects[i].credits,
            subjects[i].total,
            subjects[i].grade,
            subjects[i].point);
    }

    fprintf(fp, "-----------------------------\n");

    fclose(fp);
}

// ---------------- URL DECODING ----------------

void decodeURL(char *str) {

    char *src = str;
    char *dst = str;

    while (*src) {

        if (*src == '+') {
            *dst++ = ' ';
        }
        else if (*src == '%' && src[1] && src[2]) {

            char hex[3] = {src[1], src[2], '\0'};
            *dst++ = (char)strtol(hex, NULL, 16);

            src += 2;
        }
        else {
            *dst++ = *src;
        }

        src++;
    }

    *dst = '\0';
}

// ---------------- FORM DATA PARSER ----------------

void getValue(char *body, const char *key, char *output,
              size_t outputSize) {

    output[0] = '\0';

    char copy[BUFFER_SIZE];

    strncpy(copy, body, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *token = strtok(copy, "&");

    while (token != NULL) {

        char *equal = strchr(token, '=');

        if (equal != NULL) {

            *equal = '\0';

            if (strcmp(token, key) == 0) {

                strncpy(output, equal + 1, outputSize - 1);
                output[outputSize - 1] = '\0';

                decodeURL(output);
                return;
            }
        }

        token = strtok(NULL, "&");
    }
}

// ---------------- JSON RESPONSE ----------------

void sendResponse(SOCKET client, int status,
                  const char *contentType, const char *body) {

    char header[512];

    int length = (int)strlen(body);

    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "\r\n",
        status,
        status == 200 ? "OK" :
        status == 400 ? "Bad Request" :
        status == 404 ? "Not Found" : "Internal Server Error",
        contentType,
        length);

    send(client, header, (int)strlen(header), 0);

    int sent = 0;

    while (sent < length) {
        int n = send(client, body + sent, length - sent, 0);

        if (n <= 0) break;

        sent += n;
    }
}

// ---------------- JSON STRING ESCAPING ----------------

void jsonString(char *dest, const char *src, size_t size) {

    size_t j = 0;

    for (size_t i = 0; src[i] && j + 2 < size; i++) {

        unsigned char c = (unsigned char)src[i];

        if (c == '"' || c == '\\') {
            dest[j++] = '\\';
            dest[j++] = c;
        }
        else if (c >= 32) {
            dest[j++] = c;
        }
    }

    dest[j] = '\0';
}

// ---------------- CALCULATE API ----------------

void calculateAPI(SOCKET client, char *body) {

    char value[BUFFER_SIZE];

    getValue(body, "count", value, sizeof(value));

    int count = atoi(value);

    if (count < 1 || count > MAX_SUB) {

        sendResponse(client, 400, "application/json",
            "{\"error\":\"Enter between 1 and 10 subjects.\"}");

        return;
    }

    getValue(body, "history", value, sizeof(value));

    previousCount = 0;

    if (value[0] != '\0') {

        char *token = strtok(value, ",");

        while (token != NULL) {

            if (previousCount >= 7) {
                sendResponse(client, 400, "application/json",
                    "{\"error\":\"Maximum 7 previous semesters allowed.\"}");
                return;
            }

            char *end;
            float g = strtof(token, &end);

            while (*end == ' ') end++;

            if (end == token || *end != '\0' ||
                g < 0 || g > 10) {

                sendResponse(client, 400, "application/json",
                    "{\"error\":\"Invalid previous semester GPA.\"}");
                return;
            }

            previousGPA[previousCount++] = g;

            token = strtok(NULL, ",");
        }
    }

    for (int i = 0; i < count; i++) {

        char key[50];

        snprintf(key, sizeof(key), "s%d_name", i);
        getValue(body, key, subjects[i].name,
                 sizeof(subjects[i].name));

        snprintf(key, sizeof(key), "s%d_credits", i);
        getValue(body, key, value, sizeof(value));

        char *end;
        long credits = strtol(value, &end, 10);

        if (value[0] == '\0' || *end != '\0' ||
            credits < 1 || credits > 100) {

            sendResponse(client, 400, "application/json",
                "{\"error\":\"Invalid subject credits.\"}");
            return;
        }

        subjects[i].credits = (int)credits;

        snprintf(key, sizeof(key), "s%d_internal", i);
        getValue(body, key, value, sizeof(value));

        subjects[i].internal = strtof(value, &end);

        if (value[0] == '\0' || *end != '\0' ||
            subjects[i].internal < 0 ||
            subjects[i].internal > 50) {

            sendResponse(client, 400, "application/json",
                "{\"error\":\"Internal marks must be between 0 and 50.\"}");
            return;
        }

        snprintf(key, sizeof(key), "s%d_external", i);
        getValue(body, key, value, sizeof(value));

        subjects[i].external = strtof(value, &end);

        if (value[0] == '\0' || *end != '\0' ||
            subjects[i].external < 0 ||
            subjects[i].external > 100) {

            sendResponse(client, 400, "application/json",
                "{\"error\":\"External marks must be between 0 and 100.\"}");
            return;
        }

        if (subjects[i].name[0] == '\0') {
            sendResponse(client, 400, "application/json",
                "{\"error\":\"Subject name cannot be empty.\"}");
            return;
        }
    }

    subjectCount = count;

    semesterGPA = calculateGPA();
    updatedCGPA = calculateCGPA();

    saveRecords();

    char *out = responseBuffer;
    size_t remaining = sizeof(responseBuffer);

    int written = snprintf(out, remaining,
        "{\"gpa\":%.2f,\"cgpa\":%.2f,\"prediction\":%.2f,"
        "\"count\":%d,\"subjects\":[",
        semesterGPA, updatedCGPA, predictNext(), subjectCount);

    if (written < 0 || (size_t)written >= remaining) {
        sendResponse(client, 500, "application/json",
            "{\"error\":\"Response too large.\"}");
        return;
    }

    out += written;
    remaining -= (size_t)written;

    for (int i = 0; i < subjectCount; i++) {

        char safeName[220];

        jsonString(safeName, subjects[i].name, sizeof(safeName));

        written = snprintf(out, remaining,
            "%s{\"name\":\"%s\",\"credits\":%d,"
            "\"total\":%.1f,\"grade\":\"%s\",\"point\":%d}",
            i ? "," : "",
            safeName,
            subjects[i].credits,
            subjects[i].total,
            subjects[i].grade,
            subjects[i].point);

        if (written < 0 || (size_t)written >= remaining) {
            sendResponse(client, 500, "application/json",
                "{\"error\":\"Response too large.\"}");
            return;
        }

        out += written;
        remaining -= (size_t)written;
    }

    snprintf(out, remaining, "]}");

    sendResponse(client, 200, "application/json", responseBuffer);
}

// ---------------- TARGET GPA API ----------------

void planAPI(SOCKET client, char *body) {

    if (subjectCount == 0) {
        sendResponse(client, 400, "application/json",
            "{\"error\":\"Calculate GPA first.\"}");
        return;
    }

    char value[100];

    getValue(body, "target", value, sizeof(value));

    char *end;
    float target = strtof(value, &end);

    if (value[0] == '\0' || *end != '\0' ||
        target < 0 || target > 10) {

        sendResponse(client, 400, "application/json",
            "{\"error\":\"Enter a target GPA between 0 and 10.\"}");
        return;
    }

    float minimumMarks =
        target >= 10 ? 90 :
        target >= 9 ? 85 :
        target >= 8 ? 80 :
        target >= 7 ? 70 :
        target >= 6 ? 60 :
        target >= 5 ? 50 : 40;

    char *out = responseBuffer;
    size_t remaining = sizeof(responseBuffer);

    int written = snprintf(out, remaining,
        "{\"threshold\":%.0f,\"subjects\":[",
        minimumMarks);

    if (written < 0 || (size_t)written >= remaining) {
        sendResponse(client, 500, "application/json",
            "{\"error\":\"Response too large.\"}");
        return;
    }

    out += written;
    remaining -= (size_t)written;

    for (int i = 0; i < subjectCount; i++) {

        char safeName[220];

        jsonString(safeName, subjects[i].name, sizeof(safeName));

        float required =
            (minimumMarks - subjects[i].internal) * 2;

        if (required < 0) required = 0;

        int achievable = required <= 100;

        written = snprintf(out, remaining,
            "%s{\"name\":\"%s\",\"required\":%.1f,"
            "\"achievable\":%s}",
            i ? "," : "",
            safeName,
            required,
            achievable ? "true" : "false");

        if (written < 0 || (size_t)written >= remaining) {
            sendResponse(client, 500, "application/json",
                "{\"error\":\"Response too large.\"}");
            return;
        }

        out += written;
        remaining -= (size_t)written;
    }

    snprintf(out, remaining, "]}");

    sendResponse(client, 200, "application/json", responseBuffer);
}

// ---------------- STATIC WEBSITE FILES ----------------

void serveFile(SOCKET client, const char *path) {

    const char *filePath;
    const char *mime;

    if (strcmp(path, "/") == 0 ||
        strcmp(path, "/index.html") == 0) {

        filePath = "frontend/index.html";
        mime = "text/html; charset=utf-8";
    }
    else if (strcmp(path, "/style.css") == 0) {

        filePath = "frontend/style.css";
        mime = "text/css; charset=utf-8";
    }
    else if (strcmp(path, "/script.js") == 0) {

        filePath = "frontend/script.js";
        mime = "application/javascript; charset=utf-8";
    }
    else {

        sendResponse(client, 404, "text/plain",
                     "404 - File not found");
        return;
    }

    FILE *fp = fopen(filePath, "rb");

    if (fp == NULL) {
        sendResponse(client, 404, "text/plain",
            "File not found. Check your project folder.");
        return;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    if (size < 0 || size > 2000000) {
        fclose(fp);
        sendResponse(client, 500, "text/plain",
            "Invalid file size.");
        return;
    }

    char *data = malloc((size_t)size + 1);

    if (data == NULL) {
        fclose(fp);
        sendResponse(client, 500, "text/plain",
            "Memory allocation failed.");
        return;
    }

    size_t readSize = fread(data, 1, (size_t)size, fp);
    fclose(fp);

    data[readSize] = '\0';

    sendResponse(client, 200, mime, data);

    free(data);
}

// ---------------- HTTP REQUEST HANDLER ----------------

void handleClient(SOCKET client) {

    memset(requestBuffer, 0, sizeof(requestBuffer));

    int received = 0;

    while (received < BUFFER_SIZE - 1) {

        int n = recv(client,
            requestBuffer + received,
            BUFFER_SIZE - 1 - received, 0);

        if (n <= 0) return;

        received += n;
        requestBuffer[received] = '\0';

        char *headerEnd = strstr(requestBuffer, "\r\n\r\n");

        if (headerEnd != NULL) {

            int headerLength =
                (int)(headerEnd - requestBuffer) + 4;

            int contentLength = 0;

            char *lengthHeader =
                strstr(requestBuffer, "Content-Length:");

            if (lengthHeader != NULL) {
                contentLength = atoi(lengthHeader + 15);
            }

            if (contentLength < 0 ||
                contentLength > BUFFER_SIZE - headerLength - 1) {

                sendResponse(client, 400, "text/plain",
                    "Request body too large.");
                return;
            }

            if (received >= headerLength + contentLength) {
                break;
            }
        }
    }

    char method[10] = "";
    char path[200] = "";

    sscanf(requestBuffer, "%9s %199s", method, path);

    char *headerEnd = strstr(requestBuffer, "\r\n\r\n");

    if (headerEnd == NULL) {
        sendResponse(client, 400, "text/plain",
            "Invalid HTTP request.");
        return;
    }

    char *body = headerEnd + 4;

    if (strcmp(method, "GET") == 0) {

        serveFile(client, path);
    }
    else if (strcmp(method, "POST") == 0) {

        if (strcmp(path, "/api/calculate") == 0) {
            calculateAPI(client, body);
        }
        else if (strcmp(path, "/api/plan") == 0) {
            planAPI(client, body);
        }
        else {
            sendResponse(client, 404, "application/json",
                "{\"error\":\"API endpoint not found.\"}");
        }
    }
    else {
        sendResponse(client, 400, "text/plain",
            "Unsupported HTTP method.");
    }
}

// ---------------- MAIN SERVER ----------------

int main() {

    WSADATA wsa;

    printf("\n========================================\n");
    printf(" SMART STUDENT GPA PREDICTOR\n");
    printf(" C LANGUAGE BACKEND SERVER\n");
    printf("========================================\n");

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Winsock initialization failed.\n");
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);

    if (server == INVALID_SOCKET) {
        printf("Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    if (bind(server, (struct sockaddr *)&address,
             sizeof(address)) == SOCKET_ERROR) {

        printf("Port %d is unavailable. Close other servers.\n", PORT);

        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, 10) == SOCKET_ERROR) {

        printf("Server listen failed.\n");

        closesocket(server);
        WSACleanup();
        return 1;
    }

    printf("\nServer started successfully!\n");
    printf("Open: http://localhost:%d\n", PORT);
    printf("Press CTRL+C to stop the server.\n");
    printf("========================================\n");

    while (1) {

        SOCKET client = accept(server, NULL, NULL);

        if (client == INVALID_SOCKET) {
            continue;
        }

        handleClient(client);

        closesocket(client);
    }

    closesocket(server);
    WSACleanup();

    return 0;
}
