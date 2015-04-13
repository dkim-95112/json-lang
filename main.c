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
int ( *p )( const char *, ...) = printf; // alias
int xerr( const char *s ){ perror(s); exit(errno); }
void *xmalloc( size_t size ){
  void *rt; if(!( rt = malloc( size ) )) xerr("xmalloc"); return rt;
} // xmalloc
void *xcalloc( size_t count , size_t size ){
  void *rt; if(!( rt = calloc( count , size ) )) xerr("cmalloc"); return rt;
} // xcalloc
char *trim ( char *str ){
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
char *gettok ( char *buf , char delim ){
  int quot = 0 , brac = 0 , squar = 0 ;
  char *pc = buf , *end = buf + strlen( buf );
  while (pc < end){
    if ( *pc == '"'){
      quot ++ ;
    } else if ( *pc == '{' ){ brac ++ ;
    } else if ( *pc == '}' ){ brac -- ;
    } else if ( *pc == '[' ){ squar ++ ;
    } else if ( *pc == ']' ){ squar -- ;
    } else if ( *pc == delim ){
      if ( quot % 2 == 0 && brac == 0 && squar == 0 ){
	*pc++ = 0;
	break;
      } // if
    } // if
    pc++;
  } // while
  return pc;
} // gettok
struct tv {
  struct tv *tok ; // todo: testing
  enum {
    NUM,STR,NAM,KV,UNDEF,NUL,
    STRS,VS,KVS // STACKS
  } t ;
  union {
    char *str ; // STR
    long num ; // NUM
    struct tkv {char *key; struct tv *val;} *kv; // KV
    struct tstrs {int top; size_t sz; char **buf;} s; // STRS
    struct tvs {int top; size_t sz; struct tv **buf;} v; // VS
    struct tkvs {int top; size_t sz; struct tkv **buf;} k; // KVS
  } u ;
}; // tv
void evl( const struct tv *, struct tv *, struct tv ** );

struct tv *var( int t ){
  struct tv *v = xcalloc( 1 , sizeof(struct tv) );
  switch( v->t = t){
  case STR:
  case NAM:
  case NUM:
  case KV:
  case UNDEF:
  case NUL:
    break;
  case STRS: v->u.s.top = -1 ; break;
  case VS: v->u.v.top = -1 ; break;
  case KVS: v->u.k.top = -1 ; break;
  default: xerr("var: unknown typ"); break;
  } // switch
  return v;
} // var
struct tkv *kvar( char *key , struct tv *val ){
  struct tkv *kv = xmalloc( sizeof(struct tkv) );
  kv->key = key; kv->val = val; return kv;
} // kvar
void vfree( struct tv *v ){
  if( v ){
    switch( v->t ){
    case STRS:
      free( v->u.s.buf ); v->u.s.buf = 0; v->u.s.top = -1 ;
      break;
    case VS:
      for( int i = 0; i <= v->u.v.top; i ++ ){
	vfree( v->u.v.buf[ i ] );
      } // for
      free( v->u.v.buf ); v->u.v.buf = 0; v->u.v.top = -1 ;
      break;
    case KVS:
      for( int i = 0; i <= v->u.k.top; i ++ ){
	struct tkv *kv = v->u.k.buf[ i ];
	vfree( kv->val );
	free( kv );
      } // for
      free( v->u.k.buf ); v->u.k.buf = 0; v->u.k.top = -1 ;
      break;
    default:
      break;
    } // switch
    free( v ); 
  } // if v
} // vfree
int push( char *str , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != STRS )
      xerr("push: unexpected typ");
  } else {
    *cx = var( STRS );
  }
  struct tstrs *stac = &(*cx)->u.s;
  if ( ++stac->top >= stac->sz ){
    stac->sz += 8 * sizeof( *stac->buf );
    stac->buf = realloc( stac->buf , stac->sz );
  }
  stac->buf[ stac->top ] = str ;
  return stac->top ;
} // push
char *pop( struct tv *cx ){
  if( cx ){
    switch( cx->t ){
    case STRS:
      if( cx->u.s.top < 0 ){
	p("pop: empty\n");
      } else {
	return cx->u.s.buf[ cx->u.s.top -- ];
      }
      break;
    default:
      xerr("pop: unexpectd typ");
    } // switch
  } // if cx
  xerr("pop: cx null");
  return 0;
} // pop
int vpush( struct tv *val , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vpush: unexpected typ");
  } else {
    *cx = var( VS );
  }
  struct tvs *stac = &(*cx)->u.v;
  if ( ++ stac->top >= stac->sz ){
    stac->sz += 8 * sizeof( *stac->buf );
    stac->buf = realloc( stac->buf , stac->sz );
  }
  stac->buf[ stac->top ] = val ;
  return stac->top ;
} // vpush
struct tv *vpop ( struct tv *cx ){
  if( cx ){
    switch( cx->t ){
    case VS:
      if( cx->u.v.top < 0 ){
	p("vpop: empty\n");
      } else {
	return cx->u.v.buf[ cx->u.v.top -- ];
      }
      break;
    default:
      xerr("vpop: unexpectd typ");
    } // switch
  } // if cx
  xerr("vpop: cx null");
  return 0;
} // vpop
int vunshift( struct tv *val , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vunshift: unexpected cx typ");
  } else {
    *cx = var( VS );
  }
  struct tvs *stac = &(*cx)->u.v;
  if ( ++ stac->top >= stac->sz ){
    stac->sz += 8 * sizeof( *stac->buf );
    stac->buf = realloc( stac->buf , stac->sz );
  }
  for( int i = stac->top ; i > 0 ; i -- ){
    stac->buf[ i ] = stac->buf[ i - 1 ];
  } // for
  stac->buf[ 0 ] = val ;  // on left
  return stac->top ;
} // vpush
struct tv *vshift ( struct tv *cx ){
  if( cx ){
    switch( cx->t ){
    case VS:
      if( cx->u.v.top < 0 ){
	p("vshift: empty\n");
      } else {
	struct tv *rt = cx->u.v.buf[ 0 ];
	for( int i = 0 ; i < cx->u.v.top ; i ++ ){
	  cx->u.v.buf[ i ] = cx->u.v.buf[ i + 1 ];
	} // for
	cx->u.v.top --;
	return rt;
      } // if empty
      break;
    default:
      xerr("vshift: unexpectd typ");
    } // switch
  } // if cx
  xerr("vshift: cx null");
  return 0;
} // vshift
int kpush( struct tkv *kv , struct tv **cx ){
  if( *cx ){
    if( (*cx)->t != KVS )
      xerr("kpush: unexpectd typ");
  } else {
    *cx = var( KVS );
  } // if
  struct tkvs *stac = &(*cx)->u.k;
  if ( ++stac->top >= stac->sz ){
    stac->sz += 8 * sizeof( *stac->buf );
    stac->buf = realloc( stac->buf , stac->sz );
  }
  stac->buf[ stac->top ] = kv;
  return stac->top ;
} // kpush
struct tv *klookup( const char *key , const struct tv *cx ){
  switch( cx->t ){
  case KVS:
    for( int i = 0 ; i <= cx->u.k.top ; i ++ ){ // bottom to top ?
      struct tkv *kv = cx->u.k.buf[ i ];
      if( strcmp( kv->key , key ) == 0 ){
	return kv->val ;
      } // if
    } // for
    break;
  default:
    xerr("klookup: unexpectd cx typ");
  } // switch
  return 0;
} // klookup
struct tv *vtop( struct tv *v ){
  switch( v->t ){
  case VS :
    if( v->u.v.top < 0 ) xerr("vtop: empty stac");
    return v->u.v.buf[ v->u.v.top ];
  default:
    xerr("vtop: unexpectd vs typ");
  } // switch
  return 0;
} //vtop
struct tv *namlookup( const struct tv *nam , const struct tv *cxs ){
  switch( nam->t ){
  case NAM :
    switch( cxs->t ){
    case VS :
      for( int i = 0 ; i <= cxs->u.v.top ; i ++ ){ // left to right ?
	struct tv *v; if(( v = klookup( nam->u.str , cxs->u.v.buf[ i ] ) )){
	  return vtop( v )->tok ;
	}
      } // for
      break;
    default:
      xerr("namlookup: unexpectd cxs typ");
    } // switch
    break;
  default:
    xerr("namlookup: unexpectd nam typ");
  } // switch
  return 0 ;
} // namlookup

int trp
(
 char *sigkey , struct tv *cxs , struct tv *vargs ,
 int i , struct tv **rs , struct tv **nxrs )
{
  int rt = 0;
  if( strcmp( sigkey , "p" ) == 0 ){ // check builtins
    // todo: needs more work - put args in array ? 
    struct tv *options = vshift( vargs );
    struct tv *xfmt = klookup( "fmt" , options ); if( xfmt ){
      struct tv *vfmt = 0 ; evl( xfmt , cxs , &vfmt );
      struct tv *xarg1 = klookup( "arg1" , options ); if( xarg1 ){
	struct tv *varg1 = 0 ; evl( xarg1 , cxs , &varg1 );
	struct tv *xarg2 = klookup( "arg2" , options ); if( xarg2 ){
	  struct tv *varg2 = 0 ; evl( xarg2 , cxs , &varg2 );
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
    (*rs)->u.num ++ ;
  } else if( strcmp( sigkey , "test" ) == 0 ){
    struct tv *options = vshift( vargs );
    struct tv *xcmp = klookup( "cmp" , options ); if( xcmp ){
      struct tv *vcmp = 0 ; evl( xcmp , cxs , &vcmp );
      struct tv *xeq = klookup( "eq" , options );
      if( xeq && (*rs)->u.num == vtop( vcmp )->u.num ){
	evl( xeq , cxs , nxrs );
      } // if eq
      vfree( vcmp );
    } else
      xerr("trp: test: no cmp");
  } else if( strcmp( sigkey , "break" ) == 0 ){
    p("here");
  } else if( strcmp( sigkey , "open" ) == 0 ){
    struct tv *options = vshift( vargs );
    struct tv *xpath; if(( xpath = klookup( "path" , options ) )){
      struct tv *vpath = 0 ; evl( xpath , cxs , &vpath );
      struct tv *xoflag; if(( xoflag = klookup( "oflag" , options ) )){
	struct tv *voflag = 0 ; evl( xoflag , cxs , &voflag );
	struct tv *vnum;
	( vnum  = var( NUM ) )->u.num =
	  open( vpop( vpath )->u.str , vpop( voflag )->u.num );
	vfree( voflag );

	vpush( vnum , nxrs ); // result
      } // if oflag
      vfree( vpath );
    } // if xpath
  } else {
    struct tv *xchain; if(( xchain = klookup( sigkey , vtop( *rs ) ) )){
      struct tv *subrs = 0 ;
      switch( xchain->t ){
      case VS: // evl dot xchain
	if( vargs ){ // pass args ?
	  if( klookup( "_args" , vtop( *rs ) ) ){
	    xerr("trp: duplicate _args");
	  } else {
	    kpush( // on top
		kvar( "_args" , vshift( vargs ) ) , // reservd nam
		&(*rs)->u.v.buf[ (*rs)->u.v.top ] // last result
		);
	  } // if
	} // if

	vunshift( vtop( *rs ) , &cxs ); // left to right
	evl( xchain , cxs , &subrs );
	vshift( cxs ); // right to left

	if( subrs ){ // pass back results ?
	  vpop( *rs ); vpush( vpop( subrs ) , rs );
	}
	break;
      default:
	xerr("trp: unexpectd code typ");
      } // switch
      vfree( subrs ); // drop others ?
    } else {
      xerr("trp: no code");
    } // if code
  } // if builtins
  return rt ;
} // trp
void evl( const struct tv *xchain , struct tv *cxs , struct tv **rs ){
  if( xchain ){
    switch( xchain->t ){
    case VS: // xchain or array ?
      for( int i = 0; i <= xchain->u.v.top; i ++ ){
	struct tv *tok = xchain->u.v.buf[ i ]->tok ;
	switch( tok->t ){
	case VS : // [ array ]
	  {
	    struct tv *rarray = 0; // prepar results array
	    for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	      vpush( 0 , &rarray );
	    } // for

	    for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	      evl( tok->u.v.buf[ i ] , cxs ,
		   &rarray->u.v.buf[ i ] );
	    } // for
	    
	    vpush( rarray , rs ); // pass back results
	  }
	  break;
	case NUM: // 123
	case KVS: // { obj }
	case STR: // "string"
	  vpush( tok , rs ); // todo: escap strings here ?
	  break;
	case KV: // func()
	  { struct tv *vargs = 0 , *nxrs = 0 ;
	    struct tv *argchain; if(( argchain = tok->u.kv->val )){
	      switch( argchain->t ){
	      case VS :
		for( int i = 0 ; i <= argchain->u.v.top ; i ++ ){
		  evl( argchain->u.v.buf[ i ] , cxs , &vargs );
		} // for
		break;
	      default:
		xerr("evl: unexpectd arg typ");
	      } // switch
	    } // if
	    char *sigkey ; if( strcmp( sigkey = tok->u.kv->key , "loop" ) == 0 ){
	      struct tv *loopsig; if(( loopsig = vshift( vargs ) )){
		switch( loopsig->t ){
		case STR : sigkey = loopsig->u.str ; break;
		default: xerr("evl: unexpectd loopsig typ");
		} // switch
		int i = 0; while( trp( sigkey , cxs , vargs , i ++ , rs , &nxrs ) ){
		  // ?
		} // while
	      } else {
		xerr("evl: no loopsig key");
	      }
	    } else {
	      trp( sigkey , cxs , vargs , 0 , rs , &nxrs); // one shot
	    } // if loop
	    if( nxrs ){ // have next result ?
	      vpop( *rs ); vpush( vpop( nxrs ) , rs );
	      vfree( nxrs );
	    }
	    vfree( vargs );
	  } // vargs closure
	  break;
	case NAM: // plain nam
	  if( strcmp( tok->u.str , "sys" ) == 0 ){ // rservd
	    static struct tv *syscx = 0; if( !syscx ){ // persist in memory
	      kpush( kvar( "_sys" , 0 ) , &syscx );
	    }
	    vpush( syscx , rs );
	  } else if( strcmp( tok->u.str , "_arg" ) == 0 ){ // rservd
	    vpush( klookup( "_args" , cxs->u.v.buf[ 0 ] ) , rs ); // at bottom ?
	  } else {
	    struct tv *val ; if(( val = namlookup( tok , cxs ) )){
	      vpush( val , rs );
	    } else xerr("evl: nam not found");
	  } // if
	  break;
	default:
	  xerr("evl: unexpectd tok typ");
	  break;
	} // switch tok->t
      } // for
      break;
    default:
      xerr("evl: unexpectd typ");
    } // switch xchain->t
  } // if xchain
}// evl
struct tv *parse( char *buf ){
  char *left = trim( buf );
  struct tv *xchain = 0; while ( *left ){
    char *right = gettok ( left , '.' );
    struct tv *v = var( STR );
    v->u.str = trim ( left );
    vpush ( v , &xchain );
    left = right ;
  } // while
  for( int i = 0 ; xchain && i <= xchain->u.v.top ; i ++ ){
    left = xchain->u.v.buf[ i ]->u.str ;
    char *end = left + strlen ( left ) - 1 ;
    struct tv **tok = &xchain->u.v.buf[ i ]->tok ;
    switch( *end ){
    case '"' : // c-string
      *end = 0 ;
      ( *tok = var( STR ) )->u.str = left + 1 ;
      break;
    case ')' : // function call
      *end = 0 ;
      char *args = gettok( left , '(' );
      struct tv *vargs = 0 ; while( *args ){
	char *right = gettok ( args , ',' );
	vpush( parse( args ) , &vargs );
	args = right;
      } // while
      ( *tok = var( KV ) )->u.kv = kvar( trim( left ) , vargs );
      break;
    case '}' : // object pairs - key : vl , ...
      *end = 0 ;
      struct tv *tmp = 0;
      left = left + 1 ; while( *left ){
	char *right = gettok ( left , ',' );
	push( trim( left ) , &tmp );
	left = right;
      } // while
      for( int i = 0; tmp && i <= tmp->u.s.top; i ++ ){
	left = tmp->u.s.buf[ i ];
	char *right = gettok ( left , ':' );
	kpush( kvar( trim( left ) , parse( right ) ) , tok );
      } // while
      vfree( tmp );
      break;
    case ']' : // object pairs - key : vl , ...
      *end = 0 ;
      left = left + 1 ; while( *left ){
	char *right = gettok ( left , ',' );
	vpush( parse( left ) , tok );
	left = right;
      } // while
      break;
    default:
      if( *end >= '!' && *end <= '~' ){
	if( *left >= '0' && *left <= '9' ){
	  ( *tok = var( NUM ) )->u.num =
	    strtol( left , 0 , strncasecmp( left , "0x" , 2 ) ? 10 : 16 );
	} else {
	  ( *tok = var( NAM ) )->u.str = left ;
	} // if num
      } else {
	xerr("parse: not yet");
      } // if printable
      break;
    } // switch *end
  } // for
  return xchain;
} // parse
void dmp( struct tv * );
void kdmp( struct tkv *kv ){
  p("%s: ", kv->key );
  struct tv *v = kv->val; dmp( v && v->tok ? v->tok : v );
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
	  dmp( w && w->tok ? w->tok : w );
	}
      } p(" ]");
      break;
    case KV:
      kdmp( v->u.kv );
      break;
    case KVS:
      p("{ "); for( int i = 0 ; i <= v->u.k.top ; i ++ ){
	if( i ) p(", "); kdmp( v->u.k.buf[ i ] );
      } p(" }");
      break;
    default:
      xerr("dmp: unexpectd typ");
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
  
  struct tv *vargs = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    push( argv [ i ] , &vargs );
  } // for
  struct tv *maincx = 0; kpush( kvar( "_args" , vargs ) , &maincx );

  struct tv *cxs = 0; vunshift( maincx , &cxs ); // left to right ?
  struct tv *xchain = parse( buf );
  struct tv *rs = 0; evl( xchain , cxs , &rs );

  p("\nrs:"); dmp( rs );
  p("\nxchain:"); dmp( xchain );
  p("\ncxs:"); dmp( cxs );
  vfree( rs );
  vfree( xchain );
  vfree( cxs );

  free( buf );
  p("\nmain: end");
} // main
