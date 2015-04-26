// no licensn allowd
#include <stdio.h> // printf, fopen, fseek, ftell, fread, fclose
#include <string.h> // strlen, strcmp
#include <stdlib.h> // exit, malloc, free, realloc
#include <sys/errno.h>
#include <fcntl.h> // open
int ( *p )( const char *, ... ) = printf; // alias
int xerr( const char *s ){ fflush(NULL); perror(s); exit(errno); }
void *xmalloc( size_t size ){
  void *rt; if(!( rt = malloc( size ) )) xerr("xmalloc"); return rt;
} // xmalloc
void *xcalloc( size_t count , size_t size ){
  void *rt; if(!( rt = calloc( count , size ) )) xerr("cmalloc"); return rt;
} // xcalloc
char *trim( char *str ){
  while( *str == ' ' || *str == '\t' || *str == '\n' ) str ++ ;
  if( *str == 0 ) return str; // all spaces ?
  char *end = str + strlen( str ) - 1; // trim trailn space
  while ( str < end &&
	  ( *end == ' ' || *end == '\t' || *end == '\n' )
	  ) end--;
  *( end + 1 ) = 0; // terminatn
  return str;
} // trim
char *gettok( char *buf , char delim ){
  int quot = 0 , paren = 0 , curly = 0 , squar = 0;
  char *pc = buf; while( *pc ){
    if( *pc == delim && ( quot % 2 == 0 && !paren && !curly && !squar )){
      *pc ++ = 0; break;
    } switch( *pc ){
    case '"': quot ++ ; break;
    case '{': curly ++ ; break;
    case '}': curly -- ; break;
    case '(': paren ++ ; break;
    case ')': paren -- ; break;
    case '[': squar ++ ; break;
    case ']': squar -- ; break;
    default: break;
    } if( *pc == delim && ( quot % 2 == 0 && !paren && !curly && !squar )){
      *pc ++ = 0; break;
    } pc ++;
  } return pc;
} // gettok
int cescap ( int c ){
  switch ( c ){
  case 'a' : return 7 ;
  case 'b' : return 8 ;
  case 'n' : return 0xa ;
  case '\\' : return 0x5c ;
  case '\'' : return 0x27 ;
  case '"' : return 0x22 ;
  default : return c ;
  } // switch
} // cescap
char *escap( char *str ){
  if( *str == '"' ){ // string literal
    char *pc = str + 1;
    if( *pc == 0 ) return str;
    char *end = pc + strlen( pc ) - 1;
    if( *end != '"' )
      xerr("escap: string literal: no end dquot \"");
    *end = 0;
    while( *pc ){
      if( *pc == '\\' ){
	char *nx = pc + 1;
	if ( strchr( "bn\\\'\"" , *nx ) ){
	  *pc = cescap( *nx );
	  while(( nx[ 0 ] = nx[ 1 ] )) nx ++ ; // shift left
	} else if( *nx >= '0' && *nx <= '7' ){
	  xerr("escap: octal not yet\n");
	} else if( *nx == 'x' ){
	  xerr("escap: hex not yet\n");
	} // if escap char
      } pc ++;
    } return str + 1;
  } return str;
} // escap

struct stac {
  int top;
  size_t sz;
#define STACPAG 8
  char **buf;
};
int push( char *val , struct stac **cx ){
  if( !*cx ){
    (*cx = xcalloc( 1 , sizeof( struct stac ) ))->top = -1;
  }
  if( ( ++ (*cx)->top ) * sizeof( val ) >= (*cx)->sz ){
    (*cx)->buf =
      realloc( (*cx)->buf ,
	       (*cx)->sz += STACPAG * sizeof( val ) );
  } (*cx)->buf[ (*cx)->top ] = val;	
  return (*cx)->top ; 
} // push

typedef struct tval *tfn( const struct tval * , struct tval *);
struct tval {
  //  struct tval *x ; // child parse tree
  unsigned long flg;
#define FBRK 1
  enum val_t {
    // TOKs point to file buf, so no string malloc, for parse
    NUL = 0,
    NUM, // long
    STR, // doubl quotd "string" literal or plain sym, freed
    KEYFN, // pointer to fn
    KEYVAL, // { key : val } both key & val freed
    ARG, ARR , OBJ // ( args ) / [ array ] / { obj }
  } t ;
  union {
    long num ; // NUM
    char *str ; // STR
    struct tkeyfn { char *key; tfn *fn;} f; // KEYFN
    struct tkeyval { char *key; struct tval *val;} k; // KEYVAL
    struct tvs { int top; size_t sz; struct tval **buf;} v; // ARG/ARR/OBJ
  } u ;
}; // tval
struct tval *evl( const struct tval *, const struct tval *, struct tval * );
struct tval *fnparse( char * );
struct tval *parse( char * );

struct tval *var( int t ){
  struct tval *rt;
  switch( ( rt = xcalloc( 1 , sizeof( struct tval ) ) )->t = t ){
  case NUM:
  case STR:
  case KEYFN:
  case KEYVAL:
    break;
  case ARG:
  case ARR:
  case OBJ:
    rt->u.v.top = -1;
    break;
  default:
    xerr("var: unexp typ");
  } return rt;
} // var
struct tval *vnum( long num ){
  struct tval *rt; ( rt = var( NUM ) )->u.num = num;
  return rt;
} // vstr
struct tval *vstr( const char *str ){
  struct tval *rt; ( rt = var( STR ) )->u.str = strdup( str );
  return rt;
} // vstr
struct tval *keyfn( const char *key , tfn *fn ){
  struct tval *rt = var( KEYFN );
  rt->u.f.key = strdup( key );
  rt->u.f.fn = fn; // not freed - compild in ?
  return rt;
} // vstr
struct tval *keyval( const char *key , struct tval *val ){
  struct tval *rt = var( KEYVAL );
  rt->u.k.key = strdup( key );
  rt->u.k.val = val; // freed, caller responsible for mallocn ?
  return rt;
} // keyval
int _push( enum val_t typ , struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != typ )
      xerr("_push: unexp typ" );
  } else {
    *cx = var( typ );
  }
  if( ( ++ (*cx)->u.v.top ) * sizeof( val ) >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf =
      realloc( (*cx)->u.v.buf ,
	       (*cx)->u.v.sz += STACPAG * sizeof( val ) );
  } (*cx)->u.v.buf[ (*cx)->u.v.top ] = val;	
  return (*cx)->u.v.top ; 
} // _push
int argpush( struct tval *val , struct tval **cx ){ // ( arg )
  return _push( ARG , val , cx );
} // argpush
int arpush( struct tval *val , struct tval **cx ){ // [ arr ]
  return _push( ARR , val , cx );
} // arpush
int obpush( struct tval *val , struct tval **cx ){ // { obj }
  return _push( OBJ , val , cx );
} // obpush
struct tval *vdup( const struct tval *v ){
  struct tval *rt = 0;
  if( v ){
    switch( v->t ){
    case NUM:
      rt = vnum( v->u.num );
      break;
    case STR:
      rt = vstr( v->u.str );
      break;
    case KEYFN:
      rt = keyfn( v->u.f.key , v->u.f.fn );
      break;
    case KEYVAL:
      // val is freed, so malloc here ?
      rt = keyval( v->u.k.key , vdup( v->u.k.val ) );
      break;
    case ARG: case ARR: case OBJ:
      for( int i = 0; i <= v->u.v.top; i ++ ){
	_push( v->t , vdup( v->u.v.buf[ i ] ) , &rt );
      } break;
    default:
      xerr("vdup: unexp typ");
    } // switch v->t
  } // if v
  return rt;
} // vdup
void vfree( struct tval *v ){
  if( !v ) return;
  switch( v->t ){
  case NUM:
    break;
  case STR:
    free( v->u.str );
    v->u.str = 0;
    break;
  case KEYFN:
    free( v->u.f.key );
    v->u.f.key = 0;
    break;
  case KEYVAL:
    free( v->u.k.key );
    vfree( v->u.k.val ); // mallocd ?
    v->u.k.key = 0;
    v->u.k.val = 0;
    break;
  case ARG: // ( paren )
  case ARR: // [ array ]
  case OBJ: // { obj }
    for( int i = 0; i <= v->u.v.top; i ++ ){
      vfree( v->u.v.buf[ i ] );
      v->u.v.buf[ i ] = 0;
    }
    free( v->u.v.buf );
    v->u.v.buf = 0;
    v->u.v.top = -1;
    break;
  default:
    xerr("vfree: unexp typ");
  } free( v ); 
} // vfree

/* struct tval *vpop( struct tval *cx ){ */
/*   if( !cx ) */
/*     xerr("vpop: nul cx"); */
/*   if( cx->t != VS ) */
/*     xerr("vpop: unexp typ"); */
/*   if( cx->u.v.top < 0 ){ */
/*     p("vpop: empty\n"); */
/*   } else { */
/*     return cx->u.v.buf[ cx->u.v.top -- ]; */
/*   } return 0; */
/* } // vpop */

int unshift( struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != OBJ )
      xerr("unshift: unexp typ");
  } else {
    *cx = var( OBJ );
  } if ( ( ++ (*cx)->u.v.top ) * sizeof( val ) >= (*cx)->u.v.sz ){ // more space ?
    (*cx)->u.v.buf =
      realloc( (*cx)->u.v.buf ,
	       (*cx)->u.v.sz += STACPAG * sizeof( val ) );
  } int i = (*cx)->u.v.top; while( i ){
    (*cx)->u.v.buf[ i ] = (*cx)->u.v.buf[ i - 1 ];
    i --;
  } (*cx)->u.v.buf[ 0 ] = val;  // on left ?
  return (*cx)->u.v.top;
} // unshift
struct tval *shift( struct tval *cx ){
  if( !cx )
    xerr("shift: nul cx");
  if( cx->t != OBJ )
    xerr("shift: unexp typ");
  if( cx->u.v.top < 0 ){
    p("shift: empty\n");
  } else {
    struct tval *rt = cx->u.v.buf[ 0 ];
    for( int i = 0 ; i < cx->u.v.top ; i ++ ){
      cx->u.v.buf[ i ] = cx->u.v.buf[ i + 1 ];
    } cx->u.v.top --;
    return rt;
  } return 0;
} // shift

struct tval *klookup( const char *key , const struct tval *cx ){
  if( cx->t != OBJ )
    xerr("klookup: unexp cx typ");
  for( int i = 0 ; i <= cx->u.v.top ; i ++ ){ // bottom to top ?
    struct tval *v = cx->u.v.buf[ i ];
    if( v ){
      if( v->t != KEYVAL )
	xerr("klookup: unexp val typ");
      if( !strcmp( v->u.k.key , key ) ){
	return v->u.k.val ;
      } // if key
    } // if t
  } return 0;
} // klookup
struct tval *arlookup( const struct tval *index , const struct tval *cxs ){
  if( cxs->t != ARR)
    xerr("arlookup: unexp cxs typ");
  long num; switch( index->t ){
  case NUM :
    num = index->u.num;
    if( num < 0 ) xerr("arlookup: negative index");
    if( num > cxs->u.v.top ) xerr("arlookup: index too big");
    return cxs->u.v.buf[ num ];
    break;
  default:
    xerr("arlookup: unexp index typ");
  } // switch
  return 0 ;
} // arlookup
/* struct tval *fnlookup( const struct tval *sym , const struct tval *cx ){ */
/*   if( sym->t != KEYFN ) */
/*     xerr("fnlookup: unexp sym typ"); */
/*   if( cx->t != OBJ ) */
/*     xerr("fnlookup: unexp cx typ"); */
/*   for( int i = 0; i <= cx->u.v.top; i ++ ){ */
/*     struct tval *kval = cx->u.v.buf[ i ]; */
/*     if( kval->t != KVAL ) */
/*       xerr("fnlookup: unexp cx buf typ");; */
/*     if( kval->u.kval.val->t != FN ) */
/*       xerr("fnlookup: unexp cx kval val typ"); */
/*     if( strcmp( sym->u.kval.key , kval->u.kval.key ) == 0 ){ */
/*       return kval; */
/*     } // if */
/*   } return 0; */
/* } // fnlookup */

struct tval *g_builtins = 0; // need to initialz

struct tval *symlookup
(
 const struct tval *sym ,
 const struct tval *vargs ,
 const struct tval *cxs
 ){
  if( cxs->t != OBJ)
    xerr("symlookup: unexp cxs typ");
  switch( sym->t ){
  case KEYVAL:
    xerr("symlookup: notyet");
    //  return fnlookup( sym , g_builtins );
    break;
  case STR:
    if( !strcmp( sym->u.str , "sys" ) ){ // rservd
      static struct tval *syscx = 0; if( !syscx ){
	// tbd: temp placeholder, persist in memory ?
	obpush( keyval( "_sys" , 0 ) , &syscx );
      } return syscx;
    }
    if( strcmp( sym->u.str , "_arg" ) == 0 ){ // reservd
      return vdup( vargs );
    }
    for( int i = 0 ; i <= cxs->u.v.top ; i ++ ){ // left to right ?
      struct tval *v; 
      if(( v = klookup( sym->u.str , cxs->u.v.buf[ i ] ) )){
	return v;
      } // if
    } // for
    break;
  default:
    xerr("symlookup: unexp sym typ");
    break;
  } // switch sym->t
  return 0 ;
} // symlookup

/* struct tval *vtop( struct tval *v ){ */
/*   switch( v->t ){ */
/*   case VS : */
/*     if( v->u.v.top < 0 ) xerr("vtop: empty stac"); */
/*     return v->u.v.buf[ v->u.v.top ]; */
/*   default: */
/*     xerr("vtop: unexp vs typ"); */
/*   } // switch */
/*   return 0; */
/* } //vtop */

struct tval *builtin_p( const struct tval *vargs , struct tval *cxs ){
  if( vargs->u.v.top < 0 )
    xerr("builtin_p: no options");
  struct tval *options = vargs->u.v.buf[ 0 ];
  struct tval *xfmt = klookup( "fmt" , options ); if( xfmt ){
    struct tval *vfmt = evl( xfmt , vargs , cxs );
    struct tval *xarg1 = klookup( "arg1" , options ); if( xarg1 ){
      struct tval *varg1 = evl( xarg1 , vargs , cxs );
      struct tval *xarg2 = klookup( "arg2" , options ); if( xarg2 ){
	struct tval *varg2 = evl( xarg2 , vargs , cxs );
	p( vfmt->u.str , varg1->u.num , varg2->u.num );
	vfree( varg2 );
      } else {
	p( vfmt->u.str , varg1->u.num );
      } // if arg2
      vfree( varg1 );
    } else {
      p( "%s" , vfmt->u.str );
    } // if arg1
    vfree( vfmt );
  } else {
    xerr("builtin_p: fmt: no code");
  } // if xfmt
  return 0;
} // builtin_p
struct tval *builtin_incr( const struct tval *vargs , struct tval *cxs ){
  if( cxs->u.v.buf[0]->t != NUM )
    xerr("builtin_incr: unexp typ");
  cxs->u.v.buf[0]->u.num ++ ;
  return 0; // no malloc ?
} // builtin_incr
struct tval *builtin_test( const struct tval *vargs , struct tval *cxs ){
  if( vargs->u.v.top < 0 )
    xerr("builtin_test: no options");
  struct tval *options = vargs->u.v.buf[ 0 ];
  struct tval *xcmp = klookup( "cmp" , options ); if( xcmp ){
    struct tval *vcmp = evl( xcmp , vargs , cxs );
    struct tval *xeq = klookup( "eq" , options ); if( xeq ){
      
/*       if( xeq && vtop( vtop( *prvres ) )->u.num == vtop( vcmp )->u.num ){ */
/* 	evl( xeq , cxs , vargs , nxtres ); */
/*       } // if == */
    } // if xeq
  } else xerr("builtin_test: no xcmp");
  return 0;
} // builtin_test
struct tval *builtin_brk( const struct tval *vargs , struct tval *cxs ){
  return 0;
} // builtin_brk

/*   } else if( strcmp( sigkey , "open" ) == 0 ){ */
/*     struct tval *options = shift( vargs ); */
/*     struct tval *xpath; if(( xpath = klookup( "path" , options ) )){ */
/*       struct tval *vpath = 0 ; evl( xpath , cxs , vargs , &vpath ); */
/*       struct tval *xoflag; if(( xoflag = klookup( "oflag" , options ) )){ */
/* 	struct tval *voflag = 0 ; evl( xoflag , cxs , vargs , &voflag ); */
/* 	char *path = vtop( vpath )->u.str ; */
/* 	unsigned long oflag = vtop( voflag )->u.num; */
/* 	vpush( vnum( open( path , oflag ) ) , nxtres ); // result */
/* 	vfree( voflag ); */
/*       } // if oflag */
/*       vfree( vpath ); */
/*     } // if xpath */
//  }

struct tkeyfn g_builtinbuf[] = {
  { "p", builtin_p },
  { "incr", builtin_incr },
  { "test", builtin_test },
  { "brk", builtin_brk }
}; // g_builtinbuf

void dmp( struct tval *v ){
  if( v ){
    switch( v->t ){
    case NUM :
      p("%i", v->u.num);
      break;
    case STR:
      p("%s", v->u.str);
      break;
    case ARR:
    case ARG:
    case OBJ:
      p( "%c " , v->t == OBJ ? '{' : v->t == ARG ? '(' : '[' );
      for( int i = 0 ; i <= v->u.v.top ; i ++ ){
	if( i ) p(", ");
	dmp( v->u.v.buf[ i ] );
      }
      p( " %c" , v->t == OBJ ? '}' : v->t == ARG ? ')' : ']' );
      break;
    case KEYVAL:
      p( "%s: ", v->u.k.key );
      dmp( v->u.k.val );
      break;
    default:
      xerr("dmp: unexp typ");
    } // switch v->t
  } else p("(nul)");
} // dmp

struct tval *evl
( const struct tval *tok ,
  const struct tval *vargs ,
  struct tval *cxs
  ){
  if( tok == 0 ) return 0;
/*   if( tok->t != VS ) */
/*     xerr("evl: unexp typ"); */
/*   for( int i = 0; i <= vchain->u.v.top; i ++ ){ */

/*   } // for */


/*   struct tval *result = 0 , *last_result = 0; */
/*   if( vchain->t == KEYFN ){ */
/*     struct tval *val = vchain->u.k.val; */
/*     if( val->t == FN ){ */
/*       result = val->u.fn( vargs , cxs ); */
/*     } else { */
/*       xerr("evl: unexp keyval val typ"); */
/*     } */
/*   } else { */
/*     if( vchain->t != VS ) */
/*       xerr("evl: unexp typ"); */
/*     for( int i = 0; i <= vchain->u.v.top; i ++ ){ */
/*       struct tval *tok; switch( ( tok = vchain->u.v.buf[ i ] )->t ){ */
/*       case NUM: // 123 */
/*       case VS: // { obj } */
/* 	result = vdup( tok ); */
/* 	break; */
/*       case STR: // quotd "c-string" literal */
/* 	if( *tok->u.str == '"' ){ */
/* 	  result = vstr( escap( tok->u.str ) ); */
/* 	} else { */
/* 	  result = symlookup( tok , vargs , cxs ); */
/* 	} */
/* 	break; */
/*       case AS : // [ array ] */
/* 	for( int i = 0 ; i <= tok->u.v.top ; i ++ ){ */
/* 	  apush( evl( tok->u.v.buf[ i ] , vargs , cxs ) , &result ); */
/* 	} // for */
/* 	break; */
/*       case KEYVAL: */
/* 	if( strcmp( tok->u.kval.key , "fn" ) == 0 ){ */
/* 	  // anonymous function */
/* 	  struct tval *body = tok->u.kval.val->u.v.buf[1]; */
/* 	  for( int i = 0; i <= body->u.v.top; i ++ ){ */
/* 	    // todo: do args substitution ? */
/* 	    result = evl( body->u.v.buf[ i ] , vargs , cxs ); */
/* 	  } */
/* 	} else { */
/* 	  // calln func( v , ... ) or sym[ a-index ] */
/* 	  int ( *mypush )( struct tval * , struct tval ** ) = 0; */
/* 	  struct tval *parms_result = 0; */
/* 	  struct tval *parms; if(( parms = tok->u.kval.val )){ */
/* 	    if(!( parms->t == AS || parms->t == VS )) xerr("evl: KVAL unexp parms typ"); */
/* 	    mypush = parms->t == AS ? apush : vpush; */
/* 	    for( int i = 0 ; i <= parms->u.v.top ; i ++ ){ // eval params */
/* 	      mypush( evl( parms->u.v.buf[ i ] , vargs , cxs ) , &parms_result ); */
/* 	    } // for */
/* 	  } // if parms */
/* 	  if( parms_result && parms_result->t == AS ){ // sym [ indexes ] - do lookups */
/* 	    const struct tval *myobj = symlookup( tok , vargs , cxs ); */
/* 	    for( int i = 0; i <= parms_result->u.v.top; i ++ ){ // do lookups */
/* 	      mypush( vdup( arlookup( parms_result->u.v.buf[ i ] , myobj ) ) , &result ); */
/* 	    } // for */
/* 	  } else { // function call */
/* 	    const struct tval *evlob = 0 , *loopob = 0; */
/* 	    if( strcmp( tok->u.kval.key , "loop" ) == 0 ){ // reservd */
/* 	      struct tval *loopsig; if(( loopsig = shift( parms_result ) )){ */
/* 		if(!( loopob = symlookup( loopsig , vargs , cxs ) )){ */
/* 		  xerr("evl: no loopob"); */
/* 		} vfree( loopsig ); loopsig = 0; */
/* 	      } else { */
/* 		xerr("evl: loop: no sig "); */
/* 	      } // if loopsig */
/* 	    } else { */
/* 	      if(!( evlob = symlookup( tok , vargs , cxs ) )){ */
/* 		xerr("evl: no evlob"); */
/* 	      } */
/* 	    } // loop */
/* 	    while( 1 ){ */
/* 	      result = evl( loopob ? loopob : evlob , parms_result , cxs ); */
/* 	      if( !loopob || ( result && result->flg & FBRK ) ){ */
/* 		break; */
/* 	      } // if break */
/* 	      if( result ){ // drop last ? */
/* 		vfree( result ); result = 0; */
/* 	      } */
/* 	    } // while */
/* 	  } // if array lookup */
/* 	  vfree( parms_result ); */
/* 	} break; // case KVAL */
/*       default: */
/* 	xerr("evl: unexp tok typ"); */
/* 	break; */
/*       } // switch tok->t */
/*       if( result ){ // have result ? */
/* 	if( last_result ){ // drop last in context ? */
/* 	  vfree( shift( cxs ) ); last_result = 0; */
/* 	} */
/* 	// last in chain is new context */
/* 	unshift( last_result = result , &cxs ); result = 0; */
/*       } // if result */
/*     } // for vchain */
/*   } // if vchain->t KVAL */
/*   if( last_result ){ // remove last result from context ? */
/*     shift( cxs ); */
/*   } return last_result; */
  return 0;
}// evl

struct tval *_parse( char *lk ){ // single link
  struct tval *rt = 0;
  lk = trim( lk );
  char *end; switch( *( end = lk + strlen( lk ) - 1 ) ){
  case '"' : // "c-string" literal
    rt = vstr( lk );
    break;
  case ']' : // [ el , ... ] or sym[ el , ... ]
    { *end = 0 ;
      char *el = trim( gettok( lk , '[' ) );
      struct tval *lv = 0; while( *el ){
	char *right = gettok( el, ',' );
	arpush( parse( el ) , &lv );
	el = trim( right );
      } // while
      lk = trim( lk );
      rt = *lk ? keyval( lk , lv ) : lv;
    } break;
  case ')' : // ( arg ) or sym( arg , ... ) ?
    { *end = 0 ;
      char *el = trim( gettok( lk , '(' ) );
      struct tval *lv = 0; while( *el ){
	char *right = gettok( el, ',' );
	argpush( parse( el ) , &lv );
	el = trim( right );
      } // while el
      lk = trim( lk );
      rt = *lk ? keyval( lk , lv ) : lv;
    } break;
  case '}' : // "function{ body }" or object "{ key : val , ... }"
    { *end = 0 ;
      char *el = trim( gettok( lk , '{' ) );
      if( strcmp( lk , "fn" ) == 0 ){
	rt = keyval( lk , fnparse( el ) );
      } else {
	while( *el ){
	  char *right = gettok( el, ',' );
	  char *val = trim( gettok ( el , ':' ) );
	  el = trim( el );
	  obpush( *el ? keyval( el , parse( val ) ) : parse( val ) , &rt );
	  el = trim( right );
	} // while el
      } // if function
    } break;
  default:
    if( *end >= '!' && *end <= '~' ){
      if( *lk >= '0' && *lk <= '9' ){ // num
	rt = vnum( strtol( lk , 0 , strncasecmp( lk , "0x" , 2 ) ? 10 : 16 ) );
      } else { // plain sym
	rt = vstr( lk );
      } // if num
    } else {
      xerr("parse: not printable");
    } break;
  } return rt;
} // _parse
struct tval *parse( char *chain ){ // dot chain
  struct tval *rt = 0; while ( *chain ){
    char *right = gettok ( chain , '.' );
    argpush( _parse( chain ) , &rt );
    chain = trim( right );
  } return rt;
} // parse
struct tval *fnparse
( char *body // without brac '{}'
  ){
  struct tval *rt = 0; while( *body ){
    char *right = gettok( body , ';' ); // split on semi ';'
    obpush( parse( body ) , &rt );
    body = trim( right );
  } return rt;
} // fnparse

int main( int argc, char *argv [] ){
  if (argc < 2) xerr("file not specified");
  char *buf = 0;
  FILE *f; if(( f = fopen (argv[1], "rb") )){
    fseek (f, 0, SEEK_END);
    long n = ftell (f);
    fseek (f, 0, SEEK_SET);
    buf = xmalloc (n + 1); // one for terminatr
    fread (buf, 1, n, f);
    buf [ n ] = 0 ; // terminatn
    fclose (f);
  } else xerr("could not open file");

  struct tval *vchain = fnparse( buf );
  p("vchain:"); dmp( vchain ); p("\n");

  struct tval *args = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    argpush( vstr( argv [ i ] ) , &args );
  } p("args:"); dmp( args ); p("\n");

  struct tval *rs = evl( vchain , args , 0 );
  p("rs:"); dmp( rs ); p("\n");

  vfree( rs );
  vfree( args );
  vfree( vchain );
  free( buf );
  p("main: end\n");
} // main
