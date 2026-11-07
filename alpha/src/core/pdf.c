#include "core/pdf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *content;
    size_t len;
    size_t cap;
} AsPdfPage;

struct AsPdf {
    AsPdfPage pages[AS_PDF_MAX_PAGES];
    int page_count;
    int stroke_gray;
    double stroke_width;
    char title[128];
};

static int clamp_gray(int gray) {
    if (gray < 0) return 0;
    if (gray > 255) return 255;
    return gray;
}

static int append(AsPdf *pdf, const char *text) {
    if (!pdf || pdf->page_count <= 0 || !text) return -1;
    AsPdfPage *page = &pdf->pages[pdf->page_count - 1];
    size_t n = strlen(text);
    if (page->len + n + 1 > page->cap) {
        size_t cap = page->cap ? page->cap * 2 : 4096;
        while (cap < page->len + n + 1) cap *= 2;
        char *p = (char *)realloc(page->content, cap);
        if (!p) return -1;
        page->content = p;
        page->cap = cap;
    }
    memcpy(page->content + page->len, text, n);
    page->len += n;
    page->content[page->len] = '\0';
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
    pdf->page_count = 1;
    pdf->stroke_gray = 140;
    pdf->stroke_width = 0.5;
    snprintf(pdf->title, sizeof(pdf->title), "%s", title ? title : "AudioScope");
    return pdf;
}

int as_pdf_new_page(AsPdf *pdf) {
    if (!pdf || pdf->page_count <= 0 || pdf->page_count >= AS_PDF_MAX_PAGES) return -1;
    ++pdf->page_count;
    return 0;
}

int as_pdf_page_count(const AsPdf *pdf) {
    return pdf ? pdf->page_count : 0;
}

void as_pdf_gray(AsPdf *pdf, int gray, double width) {
    if (!pdf) return;
    pdf->stroke_gray = clamp_gray(gray);
    if (width > 0.0) pdf->stroke_width = width;
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
    snprintf(line, sizeof(line), "%.3f G %.2f w %.2f %.2f m %.2f %.2f l S\n",
             (double)pdf->stroke_gray / 255.0, pdf->stroke_width, x1, y1, x2, y2);
    append(pdf, line);
}

void as_pdf_rect(AsPdf *pdf, double x, double y, double w, double h) {
    if (!pdf) return;
    char line[160];
    snprintf(line, sizeof(line), "0.85 G %.2f %.2f %.2f %.2f re f\n", x, y, w, h);
    append(pdf, line);
}

void as_pdf_polyline(AsPdf *pdf, const double *xs, const double *ys, int n,
                     int gray, double width) {
    if (!pdf || !xs || !ys || n < 2) return;
    if (gray < 0) gray = pdf->stroke_gray;
    if (width <= 0.0) width = pdf->stroke_width;
    char line[160];
    snprintf(line, sizeof(line), "%.3f G %.2f w\n",
             (double)clamp_gray(gray) / 255.0, width);
    if (append(pdf, line) != 0) return;
    for (int i = 0; i < n; ++i) {
        snprintf(line, sizeof(line), "%.2f %.2f %s\n", xs[i], ys[i],
                 i == 0 ? "m" : "l");
        if (append(pdf, line) != 0) return;
    }
    append(pdf, "S\n");
}

int as_pdf_finish(AsPdf *pdf, const char *path) {
    if (!pdf || !path || pdf->page_count <= 0) return -1;
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    long off[2 * AS_PDF_MAX_PAGES + 4] = {0};
    int font_id = 3 + 2 * pdf->page_count;
    fprintf(f, "%%PDF-1.4\n%%\xE2\xE3\xCF\xD3\n");
    off[1] = ftell(f);
    fprintf(f, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");

    off[2] = ftell(f);
    fprintf(f, "2 0 obj\n<< /Type /Pages /Kids [");
    for (int i = 0; i < pdf->page_count; ++i)
        fprintf(f, "%s%d 0 R", i ? " " : "", 3 + 2 * i);
    fprintf(f, "] /Count %d >>\nendobj\n", pdf->page_count);

    for (int i = 0; i < pdf->page_count; ++i) {
        int page_id = 3 + 2 * i;
        int content_id = page_id + 1;
        off[page_id] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Type /Page /Parent 2 0 R "
                   "/MediaBox [0 0 595 842] "
                   "/Resources << /Font << /F1 %d 0 R >> >> "
                   "/Contents %d 0 R >>\nendobj\n",
                page_id, font_id, content_id);

        const AsPdfPage *page = &pdf->pages[i];
        const char *content = page->content ? page->content : "";
        off[content_id] = ftell(f);
        fprintf(f, "%d 0 obj\n<< /Length %zu >>\nstream\n", content_id, page->len);
        if (page->len) fwrite(content, 1, page->len, f);
        fprintf(f, "\nendstream\nendobj\n");
    }

    off[font_id] = ftell(f);
    fprintf(f, "%d 0 obj\n<< /Type /Font /Subtype /Type1 "
               "/BaseFont /Helvetica >>\nendobj\n", font_id);

    long xref = ftell(f);
    fprintf(f, "xref\n0 %d\n0000000000 65535 f \n", font_id + 1);
    for (int i = 1; i <= font_id; ++i) fprintf(f, "%010ld 00000 n \n", off[i]);
    fprintf(f, "trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%ld\n%%%%EOF\n",
            font_id + 1, xref);
    return fclose(f) == 0 ? 0 : -1;
}

void as_pdf_destroy(AsPdf *pdf) {
    if (!pdf) return;
    for (int i = 0; i < AS_PDF_MAX_PAGES; ++i) free(pdf->pages[i].content);
    free(pdf);
}
