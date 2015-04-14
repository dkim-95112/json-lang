// no licensn allowd
// todo:
// . comments
// .
//
#include <stdio.h> // printf, fopen, fseek, ftell, fread, fclose
#include <string.h> // strlen, strcmp
#include <stdlib.h> // exit, malloc, free, realloc
#include <sys/errno.h>
//#include <ctype.h>
#include <fcntl.h> // open
int ( *p )( const char *, ... ) = printf; // alias
int xerr( const char *s ){ perror(s); exit(errno); }
void *xmalloc( size_t size ){
  void *rt; if(!( rt = malloc( size ) )) xerr("xmalloc"); return rt;
} // xmalloc
void *xcalloc( size_t count , size_t size ){
  void *rt; if(!( rt = calloc( count , size ) )) xerr("cmalloc"); return rt;
} // xcalloc
char *trim( char *str ){
  while( *str == ' ' || *str == '\n' || *str == '\t' ) str ++ ;
  if(*str == 0) return str; // All spaces?
  char *end = str + strlen(str) - 1; // Trim trailing space
  while
    ( str < end &&
      ( *end == ' ' || *end == '\n' || *end == '\t' )
      ) end--;
  *(end+1) = 0; // Write new null terminator
  return str;
} // trim
char *gettok( char *buf , char delim ){
  int quot = 0 , brac = 0 , squar = 0 ;
  char *pc = buf , *end = buf + strlen( buf );
  while(pc < end){
    if( *pc == '"') quot ++ ;
    else if( *pc == '{' ) brac ++ ;
    else if( *pc == '}' ) brac -- ;
    else if( *pc == '[' ) squar ++ ;
    else if( *pc == ']' ) squar -- ;
    else if( *pc == delim ){
      if ( quot % 2 == 0 && brac == 0 && squar == 0 ){
	*pc++ = 0; break;
      } // if
    } pc ++ ;
  } // while
  return pc;
} // gettok
#define FLOOP 0x1
struct tv {
  struct tv *x ; // child parse tree
  enum {
    // TOKs point to file buf, so do no mem alloc for strings, for parsing
    NUM, // long
    STR,NAM,TOK, // quoted "c-string"
    KVAL,KTOK, // pair { c-string : val }
    STRS,TOKS, // [ c-string , ... ]
    VS, // [ val , ... ]
    KS // [ { c-string : val } , ... ]
  } t ;
  union {
    long num ; // NUM
    char *str ; // STR/NAM/TOK
    struct tk { char *key; struct tv *val;} kv; // KVAL/KTOK
    struct tstrs {int top; size_t sz; char **buf;} s; // STRS/TOKS
    struct tvs {int top; size_t sz; struct tv **buf;} v; // VS
    struct tks {int top; size_t sz; struct tk **buf;} k; // KS
  } u ;
}; // tv
void evl( const struct tv *, struct tv *, struct tv * , struct tv ** );

struct tv *var( int t ){
  struct tv *v = xcalloc( 1 , sizeof(struct tv) );
  switch( v->t = t ){
  case NUM:
  case STR: case NAM: case TOK:
  case KVAL: case KTOK:
    break;
  case STRS: case TOKS:
    v->u.s.top = -1 ; break;
  case VS: v->u.v.top = -1 ; break;
  case KS: v->u.k.top = -1 ; break;
  default:
    xerr("var: unexp typ");
  } // switch
  return v;
} // var
struct tv *vdup( const struct tv *v ){
  struct tv *rt = var( v->t );
  switch( rt->t ){
  case STR:
  case NAM:
  case NUM:
  case KVAL:
    break;
  case STRS:
    break;
  case VS:
    break;
  case KS:
    break;
  default:
    xerr("vdup: unexp typ");
  } // switch
  return rt;
} // vdup

struct tv *vstr( const char *s ){
  struct tv *rt; ( rt = var( STR ) )->u.str = strdup( s ); // mem alloc
  return rt;
} // vstr
struct tv *vtok( char *s ){
  struct tv *rt; ( rt = var( TOK ) )->u.str = s; // no mem alloc
  return rt;
} // vtok
struct tv *vkvar( const char *key , struct tv *val ){
  struct tv *rt; ( rt = var( KVAL ) )->u.kv.key = strdup( key );
  rt->u.kv.val = val ; // mem alloc ?
  return rt;
} // vkvar
struct tv *vtokvar( char *key , struct tv *val ){
  struct tv *rt; ( rt = var( KTOK ) )->u.kv.key = key;
  rt->u.kv.val = val ; // mem alloc ?
  return rt;
} // vktokvar

struct tk *kvar( const char *key , struct tv *val ){
  struct tk *rt; ( rt = xmalloc( sizeof( struct tk ) ) )->key = strdup( key );
  rt->val = val ; // mem alloc ?
  return rt;
} // kvar
struct tk *ktokvar( char *key , struct tv *val ){
  struct tk *rt; ( rt = xmalloc( sizeof( struct tk ) ) )->key = key;
  rt->val = val ; // mem alloc ?
  return rt;
} // ktokvar

int push( const char *str , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != STRS )
      xerr("push: unexp typ");
  } else *cx = var( STRS );
  if( ++ (*cx)->u.s.top >= (*cx)->u.s.sz ){
    (*cx)->u.s.buf = realloc
      ( (*cx)->u.s.buf , (*cx)->u.s.sz += 8 * sizeof( struct tv ) );
  } (*cx)->u.s.buf[ (*cx)->u.s.top ] = strdup( str ); // mem alloc
  return (*cx)->u.s.top ;
} // push
int tokpush( char *str , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != TOKS )
      xerr("tokpush: unexp typ");
  } else *cx = var( TOKS );
  if ( ++ (*cx)->u.s.top >= (*cx)->u.s.sz ){
    (*cx)->u.s.buf = realloc
      ( (*cx)->u.s.buf , (*cx)->u.s.sz += 8 * sizeof( struct tv ) );
  } (*cx)->u.s.buf[ (*cx)->u.s.top ] = str ; // no mem alloc
  return (*cx)->u.s.top ;
} // tokpush

char *pop( struct tv *cx ){
  if( cx ){
    if( cx->t != STRS )
      xerr("pop: unexp typ");
    if( cx->u.s.top < 0 ){
      p("pop: empty\n");
    } else {
      return cx->u.s.buf[ cx->u.s.top -- ];
    }
  } else xerr("pop: nul cx");
  return 0;
} // pop
struct tv *vpop( struct tv *cx ){
  if( cx ){
    if( cx->t != VS )
      xerr("vpop: unexp typ");
    if( cx->u.v.top < 0 ){
      p("vpop: empty\n");
    } else {
      return cx->u.v.buf[ cx->u.v.top -- ];
    }
  } else xerr("vpop: nul cx");
  return 0;
} // vpop

int vpush( struct tv *val , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vpush: unexp typ");
  } else *cx = var( VS );
  if ( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf = realloc
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tv ) );
  } (*cx)->u.v.buf[ (*cx)->u.v.top ] = val;
  return (*cx)->u.v.top;
} // vpush
int kpush( struct tk *kval , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != KS )
      xerr("kpush: unexp typ");
  } else *cx = var( KS );
  if( ++ (*cx)->u.k.top >= (*cx)->u.k.sz ){
    (*cx)->u.k.buf = realloc
      ( (*cx)->u.k.buf , (*cx)->u.k.sz += 8 * sizeof( struct tk ) );
  } (*cx)->u.k.buf[ (*cx)->u.k.top ] = kval;
  return (*cx)->u.k.top;
} // kpush

int vunshift( struct tv *val , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vunshift: unexp typ");
  } else *cx = var( VS );
  if ( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf = realloc
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tv ) );
  }
  for( int i = (*cx)->u.v.top ; i > 0 ; i -- ){
    (*cx)->u.v.buf[ i ] = (*cx)->u.v.buf[ i - 1 ];
  } (*cx)->u.v.buf[ 0 ] = val;  // on left ?
  return (*cx)->u.v.top;
} // vunshift
struct tv *vshift( struct tv *cx ){
  if( cx ){
    if( cx->t != VS )
      xerr("vshift: unexp typ");
    if( cx->u.v.top < 0 ){
      p("vshift: empty\n");
    } else {
      struct tv *rt = cx->u.v.buf[ 0 ];
      for( int i = 0 ; i < cx->u.v.top ; i ++ ){
	cx->u.v.buf[ i ] = cx->u.v.buf[ i + 1 ];
      } cx->u.v.top --;
      return rt;
    } // if empty
  } else xerr("vshift: nul cx");
  return 0;
} // vshift

struct tv *klookup( const char *key , const struct tv *cx ){
  switch( cx->t ){
  case KS:
    for( int i = 0 ; i <= cx->u.k.top ; i ++ ){ // bottom to top ?
      struct tk *kval = cx->u.k.buf[ i ];
      if( strcmp( kval->key , key ) == 0 ){
	return kval->val ;
      } // if
    } // for
    break;
  default:
    xerr("klookup: unexp cx typ");
  } // switch
  return 0;
} // klookup
struct tv *vtop( struct tv *v ){
  switch( v->t ){
  case VS :
    if( v->u.v.top < 0 ) xerr("vtop: empty stac");
    return v->u.v.buf[ v->u.v.top ];
  default:
    xerr("vtop: unexp vs typ");
  } // switch
  return 0;
} //vtop
void vfree( struct tv *v ){
  if( v ){
    switch( v->t ){
    case STR:
    case NAM:
      free( v->u.str ); v->u.str = 0;
      break;
    case STRS:
      free( v->u.s.buf ); v->u.s.buf = 0; v->u.s.top = -1 ;
      break;
    case VS:
      for( int i = 0; i <= v->u.v.top; i ++ ){
	vfree( v->u.v.buf[ i ] );
      } // for
      free( v->u.v.buf ); v->u.v.buf = 0; v->u.v.top = -1 ;
      break;
    case KS:
      for( int i = 0; i <= v->u.k.top; i ++ ){
	struct tk *kval = v->u.k.buf[ i ];
	free( kval->key );
	vfree( kval->val );
	free( kval );
      } // for
      free( v->u.k.buf ); v->u.k.buf = 0; v->u.k.top = -1 ;
      break;
    default:
      break;
    } // switch
    free( v ); 
  } // if v
} // vfree
struct tv *namlookup( const struct tv *nam , const struct tv *cxs ){
  switch( nam->t ){
  case NAM :
    switch( cxs->t ){
    case VS :
      for( int i = 0 ; i <= cxs->u.v.top ; i ++ ){ // left to right ?
	struct tv *v; if(( v = klookup( nam->u.str , cxs->u.v.buf[ i ] ) )){
	  return vtop( v )->x ;
	}
      } // for
      break;
    default:
      xerr("namlookup: unexp cxs typ");
    } // switch
    break;
  default:
    xerr("namlookup: unexp nam typ");
  } // switch
  return 0 ;
} // namlookup

void trp
(
 char *sigkey , struct tv *cxs , struct tv *vargs ,
 int iter , struct tv **rs , struct tv **nxrs )
{
  if( strcmp( sigkey , "p" ) == 0 ){ // check builtins
    // todo: needs more work - put args in array ? 
    struct tv *options = vshift( vargs );
    struct tv *xfmt = klookup( "fmt" , options ); if( xfmt ){
      struct tv *vfmt = 0 ; evl( xfmt , cxs , vargs , &vfmt );
      struct tv *xarg1 = klookup( "arg1" , options ); if( xarg1 ){
	struct tv *varg1 = 0 ; evl( xarg1 , cxs , vargs , &varg1 );
	struct tv *xarg2 = klookup( "arg2" , options ); if( xarg2 ){
	  struct tv *varg2 = 0 ; evl( xarg2 , cxs , vargs , &varg2 );
	  p( vpop( vfmt )->u.str , vpop( varg1 )->u.num , vpop( varg2 )->u.num );
	  vfree( varg2 );
	} else {
	  p( vpop( vfmt )->u.str , vpop( varg1 )->u.num );
	} // if arg2
	vfree( varg1 );
      } else {
	p( "%s" , vpop( vfmt )->u.str );
      } // if arg1
      vfree( vfmt );
    } else
      xerr("trp: p: no fmt");
  } else if( strcmp( sigkey , "incr" ) == 0 ){
    vtop( *rs )->u.num ++ ;
  } else if( strcmp( sigkey , "test" ) == 0 ){
    struct tv *options = vshift( vargs );
    struct tv *xcmp = klookup( "cmp" , options ); if( xcmp ){
      struct tv *vcmp = 0 ; evl( xcmp , cxs , vargs , &vcmp );
      struct tv *xeq = klookup( "eq" , options );
      if( xeq && vtop( *rs )->u.num == vtop( vcmp )->u.num ){
	evl( xeq , cxs , vargs , nxrs );
      } // if eq
    } else
      xerr("trp: test: no cmp");
  } else if( strcmp( sigkey , "brk" ) == 0 ){
    p("here");
  } else if( strcmp( sigkey , "open" ) == 0 ){
    struct tv *options = vshift( vargs );
    struct tv *xpath; if(( xpath = klookup( "path" , options ) )){
      struct tv *vpath = 0 ; evl( xpath , cxs , vargs , &vpath );
      struct tv *xoflag; if(( xoflag = klookup( "oflag" , options ) )){
	struct tv *voflag = 0 ; evl( xoflag , cxs , vargs , &voflag );
	struct tv *vnum;
	( vnum  = var( NUM ) )->u.num =
	  open( vpop( vpath )->u.str , vpop( voflag )->u.num );
	vfree( voflag );

	vpush( vnum , nxrs ); // result
      } // if oflag
      vfree( vpath );
    } // if xpath
  } else { // trp lookup
    struct tv *xstac; if(( xstac = klookup( sigkey , vtop( *rs ) ) )){
      vunshift( vtop( *rs ) , &cxs ); // left to right
      evl( xstac , cxs , vargs , nxrs );
      vshift( cxs ); // right to left
    } else {
      xerr("trp: no code");
    } // if code
  } // if builtins
} // trp
void evl( const struct tv *xstac , struct tv *cxs , struct tv *vargs , struct tv **rs ){
  if( xstac ){
    if( xstac->t != VS ) xerr("evl: unexp typ");
    for( int i = 0; i <= xstac->u.v.top; i ++ ){
      struct tv *tok = xstac->u.v.buf[ i ]->x ;
      switch( tok->t ){
      case VS : // [ array ]
	{
	  struct tv *rarray = 0; // prepar results array
	  for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	    vpush( 0 , &rarray );
	  } // for

	  for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	    evl( tok->u.v.buf[ i ] , cxs , vargs ,
		 &rarray->u.v.buf[ i ] );
	  } // for
	    
	  if( *rs ) vpop( *rs );
	  vpush( rarray , rs ); // pass back results
	}
	break;
      case NUM: // 123
	{
	  struct tv *num; ( num = var( NUM ) )->u.num = tok->u.num ;
	  vpush( num , rs );
	} break;
      case KS: // { obj }
	{
	  struct tv *kvals = 0;
	  for( int i = 0; i <= tok->u.k.top; i ++ ){
	    struct tk *kval = tok->u.k.buf[ i ];
	    kpush( kvar( kval->key , kval->val ) , &kvals );
	  } // for
	  vpush( kvals, rs );
	} break;
      case STR: // "c-string"
      case TOK: // "c-string" ?
	vpush( tok , rs ); // todo: escap strings here ?
	break;
      case KTOK: // func()
      case KVAL: // func()
	{ struct tv *nxvargs = 0 ;
	  struct tv *argstac; if(( argstac = tok->u.kv.val )){
	    if( argstac->t != VS ) xerr("evl: unexp argstac typ");
	    for( int i = 0 ; i <= argstac->u.v.top ; i ++ ){
	      evl( argstac->u.v.buf[ i ] , cxs , vargs , &nxvargs );
	    } // for
	  } // if argstac
	  struct tv *nxrs = 0 ;
	  char *sigkey ; if( strcmp( sigkey = tok->u.kv.key , "loop" ) == 0 ){
	    struct tv *loopsig; if(( loopsig = vshift( nxvargs ) )){
	      if( loopsig->t != STR ) xerr("evl: unexp loopsig typ");
	      int i = 0; while( 1 ){
		trp( loopsig->u.str , cxs , nxvargs , i ++ , rs , &nxrs );
	      }
	    } else {
	      xerr("evl: no loopsig");
	    } // if loopsig
	  } else {
	      trp( sigkey , cxs , nxvargs , 0 , rs , &nxrs );
	  } // if loop

	  if( nxrs ){ // have next result ?
	    vpop( *rs ); vpush( vpop( nxrs ) , rs );
	    vfree( nxrs );
	  }
	  vfree( nxvargs );
	} // nxvargs closure
	break;
      case NAM: // plain nam
	if( strcmp( tok->u.str , "sys" ) == 0 ){ // rservd
	  static struct tv *syscx = 0; if( !syscx ){ // persist in memory
	    kpush( kvar( "_sys" , 0 ) , &syscx ); // todo: temp placeholder ?
	  }
	  vpush( syscx , rs );
	} else if( strcmp( tok->u.str , "_arg" ) == 0 ){ // rservd
	  vpush( vargs , rs );
	} else {
	  struct tv *val ; if(( val = namlookup( tok , cxs ) )){
	    vpush( val , rs );
	  } else xerr("evl: nam not found");
	} // if
	break;
      default:
	xerr("evl: unexp tok typ");
	break;
      } // switch tok->t
    } // for
  } // if xstac
}// evl
struct tv *xparse( char *buf ){
  char *xleft = trim( buf );
  struct tv *xstac = 0; while ( *xleft ){
    char *xright = gettok ( xleft , '.' );
    vpush ( vtok( trim ( xleft ) ) , &xstac );
    xleft = xright ;
  } // while
  for( int i = 0 ; xstac && i <= xstac->u.v.top ; i ++ ){
    xleft = xstac->u.v.buf[ i ]->u.str ;
    char *xend = xleft + strlen ( xleft ) - 1 ;
    struct tv **x = &xstac->u.v.buf[ i ]->x ;
    switch( *xend ){
    case '"' : // "c-string"
      *x = vtok( xleft );
      break;
    case ')' : // func() call
      *xend = 0 ;
      char *xargs = gettok( xleft , '(' );
      struct tv *vtokargs = 0 ; while( *xargs ){
	char *xright = gettok ( xargs , ',' );
	vpush( xparse( xargs ) , &vtokargs );
	xargs = xright;
      } // while
      *x = vtokvar( trim( xleft ) , vtokargs );
      break;
    case '}' : // object pairs [ { key : val } , ... ]
      *xend = 0 ;
      struct tv *toktmp = 0;
      xleft = xleft + 1 ; while( *xleft ){
	char *xright = gettok ( xleft , ',' );
	tokpush( trim( xleft ) , &toktmp );
	xleft = xright;
      } // while
      for( int i = 0; toktmp && i <= toktmp->u.s.top; i ++ ){
	xleft = toktmp->u.s.buf[ i ];
	char *xright = gettok ( xleft , ':' );
	kpush( ktokvar( trim( xleft ) , xparse( xright ) ), x );
      } // while
      break;
    case ']' : // [ c-string , ... ]
      *xend = 0 ;
      xleft = xleft + 1 ; while( *xleft ){
	char *xright = gettok ( xleft , ',' );
	vpush( xparse( xleft ) , x );
	xleft = xright;
      } // while
      break;
    default:
      if( *xend >= '!' && *xend <= '~' ){
	if( *xleft >= '0' && *xleft <= '9' ){
	  ( *x = var( NUM ) )->u.num =
	    strtol( xleft , 0 , strncasecmp( xleft , "0x" , 2 ) ? 10 : 16 );
	} else {
	  *x = vtok( xleft );
	} // if num
      } else {
	xerr("parse: not yet");
      } // if printable
      break;
    } // switch *end
  } // for
  return xstac;
} // parse
void dmp( struct tv * );
void kdmp( struct tk *kval ){
  p("%s: ", kval->key );
  struct tv *v = kval->val; dmp( v && v->x ? v->x : v );
} // kdmp
void dmp( struct tv *v ){
  if( v ){
    switch( v->t ){
    case NUM :
      p("%i", v->u.num);
      break;
    case STR:
      p("\"%s\"", v->u.str);
      break;
    case NAM:
      p("%s", v->u.str);
      break;
    case STRS:
      p("[ ");
      for( int i = 0 ; i <= v->u.s.top ; i ++ ){
	if( i ) p(", "); p("\"%s\"", v->u.s.buf[ i ] );
      }
      p(" ]");
      break;
    case VS:
      p("[ "); for( int i = 0 ; i <= v->u.v.top ; i ++ ){
	if( i ) p(", "); {
	  struct tv *w = v->u.v.buf[ i ];
	  dmp( w && w->x ? w->x : w );
	}
      } p(" ]");
      break;
    case KVAL:
      kdmp( &v->u.kv );
      break;
    case KS:
      p("{ "); for( int i = 0 ; i <= v->u.k.top ; i ++ ){
	if( i ) p(", "); kdmp( v->u.k.buf[ i ] );
      } p(" }");
      break;
    default:
      xerr("dmp: unexp typ");
    } // switch
  } else {
    p("(nul)");
  } // if v
} // dmp
int main ( int argc, char *argv [] )
{
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
  } else {
    xerr("could not open file");
  }
  
  struct tv *xstac = xparse( buf );

  struct tv *vargs = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    push( argv [ i ] , &vargs );
  } // for
  struct tv *rs = 0; evl( xstac , 0 , vargs , &rs );

  p("\nrs:"); dmp( rs );
  p("\nxstac:"); dmp( xstac );
  p("\nvargs:"); dmp( vargs );
  vfree( rs );
  vfree( xstac );
  vfree( vargs );
  free( buf );
  p("\nmain: end");
} // main
