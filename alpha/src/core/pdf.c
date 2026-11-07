#include "core/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AsPdf {
    char *content;
    size_t len;
    size_t cap;
    char title[128];
};

static int append(AsPdf *pdf, const char *text) {
    size_t n = strlen(text);
    if (pdf->len + n + 1 > pdf->cap) {
        size_t cap = pdf->cap ? pdf->cap * 2 : 4096;
        while (cap < pdf->len + n + 1) cap *= 2;
        char *p = (char *)realloc(pdf->content, cap);
        if (!p) return -1;
        pdf->content = p;
        pdf->cap = cap;
    }
    memcpy(pdf->content + pdf->len, text, n);
    pdf->len += n;
    pdf->content[pdf->len] = '\0';
    return 0;
}

static void escape(char *dst, size_t cap, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j + 3 < cap; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '(' || c == ')' || c == '\\') {
            dst[j++] = '\\';
            dst[j++] = (char)c;
        } else if (c < 32 || c > 126) {
            dst[j++] = '?';
        } else {
            dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

AsPdf *as_pdf_create(const char *title) {
    AsPdf *pdf = (AsPdf *)calloc(1, sizeof(*pdf));
    if (!pdf) return NULL;
    snprintf(pdf->title, sizeof(pdf->title), "%s", title ? title : "AudioScope");
    return pdf;
}

void as_pdf_text(AsPdf *pdf, double x, double y, double size, const char *text) {
    if (!pdf || !text) return;
    char esc[1024];
    escape(esc, sizeof(esc), text);
    char line[1200];
    snprintf(line, sizeof(line), "BT /F1 %.1f Tf %.2f %.2f Td (%s) Tj ET\n",
             size, x, y, esc);
    append(pdf, line);
}

void as_pdf_line(AsPdf *pdf, double x1, double y1, double x2, double y2) {
    if (!pdf) return;
    char line[160];
    snprintf(line, sizeof(line), "0.55 G 0.5 w %.2f %.2f m %.2f %.2f l S\n",
             x1, y1, x2, y2);
    append(pdf, line);
}

void as_pdf_rect(AsPdf *pdf, double x, double y, double w, double h) {
    if (!pdf) return;
    char line[160];
    snprintf(line, sizeof(line), "0.85 G %.2f %.2f %.2f %.2f re f\n", x, y, w, h);
    append(pdf, line);
}

int as_pdf_finish(AsPdf *pdf, const char *path) {
    if (!pdf || !path) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    long off[6] = {0};
    fprintf(f, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
    off[1] = ftell(f);
    fprintf(f, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    off[2] = ftell(f);
    fprintf(f, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    off[3] = ftell(f);
    fprintf(f, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] "
               "/Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>\nendobj\n");
    const char *content = pdf->content ? pdf->content : "";
    size_t clen = pdf->len;
    off[4] = ftell(f);
    fprintf(f, "4 0 obj\n<< /Length %zu >>\nstream\n", clen);
    fwrite(content, 1, clen, f);
    fprintf(f, "\nendstream\nendobj\n");
    off[5] = ftell(f);
    fprintf(f, "5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");
    long xref = ftell(f);
    fprintf(f, "xref\n0 6\n0000000000 65535 f \n");
    for (int i = 1; i <= 5; ++i) fprintf(f, "%010ld 00000 n \n", off[i]);
    fprintf(f, "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n", xref);
    fclose(f);
    return 0;
}

void as_pdf_destroy(AsPdf *pdf) {
    if (!pdf) return;
    free(pdf->content);
    free(pdf);
}
