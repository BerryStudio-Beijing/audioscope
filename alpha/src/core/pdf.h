/* AudioScope by BerryStudio 0.6-alpha — minimal single-page PDF writer. */
#ifndef AS_CORE_PDF_H
#define AS_CORE_PDF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AsPdf AsPdf;

AsPdf *as_pdf_create(const char *title);
void as_pdf_text(AsPdf *pdf, double x, double y, double size, const char *text);
void as_pdf_line(AsPdf *pdf, double x1, double y1, double x2, double y2);
void as_pdf_rect(AsPdf *pdf, double x, double y, double w, double h);
int  as_pdf_finish(AsPdf *pdf, const char *path);
void as_pdf_destroy(AsPdf *pdf);

#ifdef __cplusplus
}
#endif

#endif
