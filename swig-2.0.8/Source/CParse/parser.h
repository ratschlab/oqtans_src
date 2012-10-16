#define ID 257
#define HBLOCK 258
#define POUND 259
#define STRING 260
#define INCLUDE 261
#define IMPORT 262
#define INSERT 263
#define CHARCONST 264
#define NUM_INT 265
#define NUM_FLOAT 266
#define NUM_UNSIGNED 267
#define NUM_LONG 268
#define NUM_ULONG 269
#define NUM_LONGLONG 270
#define NUM_ULONGLONG 271
#define NUM_BOOL 272
#define TYPEDEF 273
#define TYPE_INT 274
#define TYPE_UNSIGNED 275
#define TYPE_SHORT 276
#define TYPE_LONG 277
#define TYPE_FLOAT 278
#define TYPE_DOUBLE 279
#define TYPE_CHAR 280
#define TYPE_WCHAR 281
#define TYPE_VOID 282
#define TYPE_SIGNED 283
#define TYPE_BOOL 284
#define TYPE_COMPLEX 285
#define TYPE_TYPEDEF 286
#define TYPE_RAW 287
#define TYPE_NON_ISO_INT8 288
#define TYPE_NON_ISO_INT16 289
#define TYPE_NON_ISO_INT32 290
#define TYPE_NON_ISO_INT64 291
#define LPAREN 292
#define RPAREN 293
#define COMMA 294
#define SEMI 295
#define EXTERN 296
#define INIT 297
#define LBRACE 298
#define RBRACE 299
#define PERIOD 300
#define CONST_QUAL 301
#define VOLATILE 302
#define REGISTER 303
#define STRUCT 304
#define UNION 305
#define EQUAL 306
#define SIZEOF 307
#define MODULE 308
#define LBRACKET 309
#define RBRACKET 310
#define BEGINFILE 311
#define ENDOFFILE 312
#define ILLEGAL 313
#define CONSTANT 314
#define NAME 315
#define RENAME 316
#define NAMEWARN 317
#define EXTEND 318
#define PRAGMA 319
#define FEATURE 320
#define VARARGS 321
#define ENUM 322
#define CLASS 323
#define TYPENAME 324
#define PRIVATE 325
#define PUBLIC 326
#define PROTECTED 327
#define COLON 328
#define STATIC 329
#define VIRTUAL 330
#define FRIEND 331
#define THROW 332
#define CATCH 333
#define EXPLICIT 334
#define USING 335
#define NAMESPACE 336
#define NATIVE 337
#define INLINE 338
#define TYPEMAP 339
#define EXCEPT 340
#define ECHO 341
#define APPLY 342
#define CLEAR 343
#define SWIGTEMPLATE 344
#define FRAGMENT 345
#define WARN 346
#define LESSTHAN 347
#define GREATERTHAN 348
#define DELETE_KW 349
#define LESSTHANOREQUALTO 350
#define GREATERTHANOREQUALTO 351
#define EQUALTO 352
#define NOTEQUALTO 353
#define QUESTIONMARK 354
#define TYPES 355
#define PARMS 356
#define NONID 357
#define DSTAR 358
#define DCNOT 359
#define TEMPLATE 360
#define OPERATOR 361
#define COPERATOR 362
#define PARSETYPE 363
#define PARSEPARM 364
#define PARSEPARMS 365
#define CAST 366
#define LOR 367
#define LAND 368
#define OR 369
#define XOR 370
#define AND 371
#define LSHIFT 372
#define RSHIFT 373
#define PLUS 374
#define MINUS 375
#define STAR 376
#define SLASH 377
#define MODULO 378
#define UMINUS 379
#define NOT 380
#define LNOT 381
#define DCOLON 382
typedef union {
  char  *id;
  List  *bases;
  struct Define {
    String *val;
    String *rawval;
    int     type;
    String *qualifier;
    String *bitfield;
    Parm   *throws;
    String *throwf;
  } dtype;
  struct {
    char *type;
    String *filename;
    int   line;
  } loc;
  struct {
    char      *id;
    SwigType  *type;
    String    *defarg;
    ParmList  *parms;
    short      have_parms;
    ParmList  *throws;
    String    *throwf;
  } decl;
  Parm         *tparms;
  struct {
    String     *method;
    Hash       *kwargs;
  } tmap;
  struct {
    String     *type;
    String     *us;
  } ptype;
  SwigType     *type;
  String       *str;
  Parm         *p;
  ParmList     *pl;
  int           intvalue;
  Node         *node;
} YYSTYPE;
extern YYSTYPE yylval;
