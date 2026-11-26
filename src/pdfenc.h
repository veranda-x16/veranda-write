/* Veranda Write: the PDF's encoding: the 40 bytes where ISO-8859-16 is
   not WinAnsiEncoding (tools/afmtab.py makes it; pdfw.h includes it).
   Copyright (c) 2026 Marcin Spoczynski. MIT licence (see LICENSE). */
/* (1.6: a macro: the X16 keeps it in overlay 28, v/tbui.c's pdf_enc,
   for overlay 15's room) */
#define PDF_DIFF \
    "161 /Aogonek /aogonek /Lslash /Euro /quotedblbase /Scaron 168 /scaron " \
    "170 /Scommaaccent 172 /Zacute 174 /zacute /Zdotaccent 178 /Ccaron " \
    "/lslash /Zcaron /quotedblright 184 /zcaron /ccaron /scommaaccent 188 " \
    "/OE /oe /Ydieresis /zdotaccent 195 /Abreve 197 /Cacute 208 /Dcroat " \
    "/Nacute 213 /Ohungarumlaut 215 /Sacute /Uhungarumlaut 221 /Eogonek " \
    "/Tcommaaccent 227 /abreve 229 /cacute 240 /dcroat /nacute 245 " \
    "/ohungarumlaut 247 /sacute /uhungarumlaut 253 /eogonek /tcommaaccent"

/* the encoding object's text around them (object 3: pdf.c's fonts) */
#define PDF_ENC0 "<< /Type /Encoding /BaseEncoding /WinAnsiEncoding /Differences ["
#define PDF_ENC1 "] >>\n"
