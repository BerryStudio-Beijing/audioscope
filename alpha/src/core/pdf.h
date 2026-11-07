/* AudioScope by BerryStudio 0.9-alpha — minimal multi-page PDF writer. */
#ifndef AS_CORE_PDF_H
#define AS_CORE_PDF_H

#ifdef __cplusplus
extern "C" {
#endif

#define AS_PDF_MAX_PAGES 16

typedef struct AsPdf AsPdf;

AsPdf *as_pdf_create(const char *title);
int  as_pdf_new_page(AsPdf *pdf);
int  as_pdf_page_count(const AsPdf *pdf);
void as_pdf_gray(AsPdf *pdf, int gray, double width);
void as_pdf_text(AsPdf *pdf, double x, double y, double size, const char *text);
void as_pdf_line(AsPdf *pdf, double x1, double y1, double x2, double y2);
void as_pdf_rect(AsPdf *pdf, double x, double y, double w, double h);
/* gray < 0 or width <= 0 keep the current stroke state. */
void as_pdf_polyline(AsPdf *pdf, const double *xs, const double *ys, int n,
                     int gray, double width);
int  as_pdf_finish(AsPdf *pdf, const char *path);
void as_pdf_destroy(AsPdf *pdf);

#ifdef __cplusplus
}
#endif

#endif
