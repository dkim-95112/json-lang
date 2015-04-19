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
int xerr( const char *s ){ fflush(NULL); perror(s); exit(errno); }
void *xmalloc( size_t size ){
  void *rt; if(!( rt = malloc( size ) )) xerr("xmalloc"); return rt;
} // xmalloc
void *xcalloc( size_t count , size_t size ){
  void *rt; if(!( rt = calloc( count , size ) )) xerr("cmalloc"); return rt;
} // xcalloc
char *trim( char *str ){
  while( *str == ' ' || *str == '\n' || *str == '\t' ) str ++ ;
  if( *str == 0 ) return str; // All spaces?
  char *end = str + strlen( str ) - 1; // Trim trailing space
  while
    ( str < end &&
      ( *end == ' ' || *end == '\n' || *end == '\t' )
      ) end--;
  *( end + 1 ) = 0; // Write new null terminator
  return str;
} // trim
char *gettok( char *buf , char delim ){
  int quot = 0 , paren = 0 , curly = 0 , squar = 0 ;
  char *pc = buf , *end = buf + strlen( buf );
  while(pc < end){
    if( *pc == delim ){
      if ( quot % 2 == 0 && paren == 0 && curly == 0 && squar == 0 ){
	*pc++ = 0; break;
      } // if
    } else if( *pc == '"') quot ++ ;
    else if( *pc == '{' ) curly ++ ;
    else if( *pc == '}' ) curly -- ;
    else if( *pc == '(' ) paren ++ ;
    else if( *pc == ')' ) paren -- ;
    else if( *pc == '[' ) squar ++ ;
    else if( *pc == ']' ) squar -- ;
    pc ++ ;
  } // while
  return pc;
} // gettok
int escap ( int c ){
  switch ( c ){
  case 'a' : return 7 ;
  case 'b' : return 8 ;
  case 'n' : return 0xa ;
  case '\\' : return 0x5c ;
  case '\'' : return 0x27 ;
  case '"' : return 0x22 ;
  default : return c ;
  } // switch
} // escap

#define FLOOP 0x1
struct tval {
  struct tval *x ; // child parse tree
  enum {
    // TOKs point to file buf, so no string malloc, for parse
    NUL = 0,
    NUM, // long
    STR,NAM,TOK, // "string" literal/name/no malloc
    STRS,NAMS,TOKS, // stack
    KVAL,KTOK, // { key : val } w/wo malloc
    KS,XS, // stack
    VS,AS // ( paren ), [ array ]
  } t ;
  union {
    long num ; // NUM
    char *str ; // STR/NAM/TOK
    struct tstrs { int top; size_t sz; char **buf; } s; // STRS/NAMS/TOKS

    struct tkeyval { char *key; struct tval *val; } kval; // KVAL
    struct tks { int top; size_t sz; struct tkeyval **buf; } k; // KS

    struct tkeytok { char *key; struct tval *val; } ktok; // KTOK
    struct txs { int top; size_t sz; struct tkeytok **buf; } x; // XS

    struct tvs { int top; size_t sz; struct tval **buf; } v; // VS/AS
  } u ;
}; // tval
void evl( const struct tval *, struct tval *, struct tval * , struct tval ** );
struct tval *vdup( const struct tval * );

struct tval *var( int t ){
  struct tval *rt = xcalloc( 1 , sizeof( struct tval ) );
  switch( rt->t = t ){
  case NUM:
  case STR: case NAM: case TOK:
  case KVAL: case KTOK:
    break;
  case STRS: case NAMS: case TOKS:
    rt->u.s.top = -1 ; break;
  case KS: rt->u.k.top = -1 ; break;
  case XS: rt->u.x.top = -1 ; break;
  case VS: case AS:
    rt->u.v.top = -1 ; break;
  default:
    xerr("var: unexp typ");
  } // switch
  return rt;
} // var

struct tval *vstr( const char *str ){
  struct tval *rt; ( rt = var( STR ) )->u.str = strdup( str );
  return rt;
} // vstr
struct tval *vtok( char *str ){
  struct tval *rt; ( rt = var( TOK ) )->u.str = str;
  return rt;
} // vtok

struct tkeyval *keyvar( const char *key , struct tval *val ){
  struct tkeyval *rt = xmalloc( sizeof( struct tkeyval ) );
  rt->key = strdup( key );
  rt->val = val;
  return rt;
} // keyvar
struct tval *vkeyvar( const char *key , struct tval *val ){
  struct tval *rt = var( KVAL );
  rt->u.kval.key = strdup( key );
  rt->u.kval.val = val;
  return rt;
} // vkeyvar
struct tkeytok *keytokvar( char *key , struct tval *val ){
  struct tkeytok *rt = xmalloc( sizeof( struct tkeytok ) );
  rt->key = key;
  rt->val = val;
  return rt;
} // keytokvar
struct tval *vkeytokvar( char *key , struct tval *val ){
  struct tval *rt = var( KTOK );
  rt->u.ktok.key = key ;
  rt->u.ktok.val = val;
  return rt;
} // vkeytokvar

#define PUSH( PRE , TYP , CTYP , UTYP )				\
  int PRE ## push( CTYP val , struct tval **cx ){			\
    if( *cx ){								\
      if( (*cx)->t != TYP )						\
	xerr( #PRE "push: unexp typ" );					\
    } else *cx = var( TYP );						\
    if( ++ (*cx)->u.UTYP.top >= (*cx)->u.UTYP.sz ){			\
      (*cx)->u.UTYP.buf = realloc					\
	( (*cx)->u.UTYP.buf , (*cx)->u.UTYP.sz += 8 * sizeof( CTYP ) );	\
    } (*cx)->u.UTYP.buf[ (*cx)->u.UTYP.top ] = val;		\
    return (*cx)->u.UTYP.top ;						\
  }
PUSH( s   , STRS , char * , s )
PUSH( tok , TOKS , char * , s )
PUSH( key , KS , struct tkeyval * , k )
PUSH( keytok , XS , struct tkeytok * , x )
PUSH( a   , AS , struct tval * , v )
PUSH( v   , VS , struct tval * , v )

struct tval *vdup( const struct tval *v ){
  struct tval *rt = var( v->t );
  switch( rt->t ){
  case NUM:
    rt->u.num = v->u.num;
    break;
  case STR: case NAM:
    rt->u.str = strdup( v->u.str );
    break;
  case STRS: case NAMS:
    rt->u.s.top = v->u.s.top;
    rt->u.s.sz = v->u.s.sz;
    rt->u.s.buf = xcalloc( rt->u.s.sz , sizeof( *rt->u.s.buf ) );
    for( int i = 0; i <= rt->u.s.top; i ++ ){
      rt->u.s.buf[ i ] = strdup( v->u.s.buf[ i ] );
    } break;
  case KVAL:
    rt->u.kval.key = strdup( v->u.kval.key );
    rt->u.kval.val = vdup( v->u.kval.val );
    break;
  case KS:
    rt->u.k.top = v->u.k.top;
    rt->u.k.sz = v->u.k.sz;
    rt->u.k.buf = xcalloc( rt->u.k.sz , sizeof( *rt->u.k.buf ) );
    for( int i = 0; i <= rt->u.k.top; i ++ ){
      struct tkeyval *val = v->u.k.buf[ i ];
      rt->u.k.buf[ i ] = keyvar( val->key , val->val );
    } break;
  case VS: case AS:
    rt->u.v.top = v->u.v.top;
    rt->u.v.sz = v->u.v.sz;
    rt->u.v.buf = xcalloc( rt->u.v.sz , sizeof( *rt->u.v.buf ) );
    for( int i = 0; i <= rt->u.v.top; i ++ ){
      rt->u.v.buf[ i ] = vdup( v->u.v.buf[ i ] );
    } break;
  default:
    xerr("vdup: unexp typ");
  } // switch rt->t
  return rt;
} // vdup

char *pop( struct tval *cx ){
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
struct tval *vpop( struct tval *cx ){
  if( cx ){
    if( cx->t != VS )
      xerr("vpop: unexp typ");
    if( cx->u.v.top < 0 ){
      p("vpop: empty\n");
    } else {
      return cx->u.v.buf[ cx->u.v.top -- ];
    }
  } else
    xerr("vpop: nul cx");
  return 0;
} // vpop

int vunshift( struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vunshift: unexp typ");
  } else *cx = var( VS );
  if ( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf = realloc
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tval *) );
  }
  for( int i = (*cx)->u.v.top ; i > 0 ; i -- ){
    (*cx)->u.v.buf[ i ] = (*cx)->u.v.buf[ i - 1 ];
  } (*cx)->u.v.buf[ 0 ] = val;  // on left ?
  return (*cx)->u.v.top;
} // vunshift
struct tval *vshift( struct tval *cx ){
  if( cx ){
    if( cx->t != VS )
      xerr("vshift: unexp typ");
    if( cx->u.v.top < 0 ){
      p("vshift: empty\n");
    } else {
      struct tval *rt = cx->u.v.buf[ 0 ];
      for( int i = 0 ; i < cx->u.v.top ; i ++ ){
	cx->u.v.buf[ i ] = cx->u.v.buf[ i + 1 ];
      } cx->u.v.top --;
      return rt;
    } // if empty
  } else xerr("vshift: nul cx");
  return 0;
} // vshift

struct tval *klookup( const char *key , const struct tval *cx ){
  switch( cx->t ){
  case KS:
    for( int i = 0 ; i <= cx->u.k.top ; i ++ ){ // bottom to top ?
      struct tkeyval *kval = cx->u.k.buf[ i ];
      if( strcmp( kval->key , key ) == 0 ){
	return kval->val ;
      } // if
    } // for
    break;
  default:
    xerr("klookup: unexp cx typ");
  } // switch cx->t
  return 0;
} // klookup
struct tval *vtop( struct tval *v ){
  switch( v->t ){
  case VS :
    if( v->u.v.top < 0 ) xerr("vtop: empty stac");
    return v->u.v.buf[ v->u.v.top ];
  default:
    xerr("vtop: unexp vs typ");
  } // switch
  return 0;
} //vtop
struct tval *scoplookup( const struct tval *nam , const struct tval *cxs ){
  if( cxs->t != VS)
    xerr("scoplookup: unexp cxs typ");
  switch( nam->t ){
  case NAM :
    for( int i = 0 ; i <= cxs->u.v.top ; i ++ ){ // left to right ?
      struct tval *v; if(( v = klookup( nam->u.str , cxs->u.v.buf[ i ] ) )){
	return vtop( v )->x ;
      }
    } // for
    break;
  default:
    xerr("scoplookup: unexp nam typ");
  } // switch
  return 0 ;
} // scoplookup
struct tval *arrlookup( const struct tval *index , const struct tval *cxs ){
  if( cxs->t != VS)
    xerr("arrlookup: unexp cxs typ");
  long num; switch( index->t ){
  case NUM :
    num = index->u.num;
    if( num < 0 )
      xerr("arrlookup: negative index");
    if( num > cxs->u.v.top )
      xerr("arrlookup: index too big");
    return cxs->u.v.buf[ num ];
    break;
  default:
    xerr("arrlookup: unexp index typ");
  } // switch
  return 0 ;
} // arrlookup

void vfree( struct tval *v ){
  if( v ){
    switch( v->t ){
    case NUM:
      break;

    case STR:
    case NAM:
      free( v->u.str ); // mem alloc
    case TOK:
      v->u.str = 0;
      break;

    case STRS:
      for( int i = 0 ; i <= v->u.s.top ; i ++ )
	free( v->u.s.buf[ i ] ); // mem alloc
    case TOKS:
      for( int i = 0 ; i <= v->u.s.top ; i ++ )
	v->u.s.buf[ i ] = 0;
      free( v->u.s.buf ); v->u.s.buf = 0;
      v->u.s.top = -1;
      break;

    case KVAL:
      free( v->u.kval.key ); v->u.kval.key = 0; // mem alloc
      vfree( v->u.kval.val ); v->u.kval.val = 0;
      break;
    case KS:
      for( int i = 0; i <= v->u.k.top; i ++ ){
	struct tkeyval *kval = v->u.k.buf[ i ];
	free( kval->key ); kval->key = 0; // mem alloc
	vfree( kval->val ); kval->val = 0;
	free( kval ); kval = 0;
      } free( v->u.k.buf ); v->u.k.buf = 0;
      v->u.k.top = -1 ;
      break;

    case KTOK:
      v->u.ktok.key = 0; // no mem alloc
      vfree( v->u.ktok.val ); v->u.ktok.val = 0;
      break;
    case XS:
      for( int i = 0; i <= v->u.x.top; i ++ ){
	struct tkeytok *ktok = v->u.x.buf[ i ];
	ktok->key = 0; // no mem alloc
	vfree( ktok->val ); ktok->val = 0;
	free( ktok ); ktok = 0;
      } free( v->u.x.buf ); v->u.x.buf = 0;
      v->u.x.top = -1;
      break;

    case VS: // ( paren )
    case AS: // [ array ]
      for( int i = 0; i <= v->u.v.top; i ++ ){
	vfree( v->u.v.buf[ i ] ); v->u.v.buf[ i ] = 0;
      } free( v->u.v.buf ); v->u.v.buf = 0;
      v->u.v.top = -1;
      break;

    default:
      xerr("vfree: unexp typ");
      break;
    } // switch
    free( v ); 
  } // if v
} // vfree

void trp
(
 char *sigkey , struct tval *cxs , struct tval *vargs ,
 int iter , struct tval **rs , struct tval **nxrs )
{
  if( strcmp( sigkey , "p" ) == 0 ){ // check builtins
    // todo: needs more work - put args in array ? 
    struct tval *options = vshift( vargs );
    struct tval *xfmt = klookup( "fmt" , options ); if( xfmt ){
      struct tval *vfmt = 0 ; evl( xfmt , cxs , vargs , &vfmt );
      struct tval *xarg1 = klookup( "arg1" , options ); if( xarg1 ){
	struct tval *varg1 = 0 ; evl( xarg1 , cxs , vargs , &varg1 );
	struct tval *xarg2 = klookup( "arg2" , options ); if( xarg2 ){
	  struct tval *varg2 = 0 ; evl( xarg2 , cxs , vargs , &varg2 );
	  p( vtop( vfmt )->u.str , vtop( varg1 )->u.num , vtop( varg2 )->u.num );
	  vfree( varg2 );
	} else {
	  p( vtop( vfmt )->u.str , vtop( varg1 )->u.num );
	} // if arg2
	vfree( varg1 );
      } else {
	p( "%s" , vtop( vfmt )->u.str );
      } // if arg1
      vfree( vfmt );
    } else
      xerr("trp: p: no fmt");
  } else if( strcmp( sigkey , "incr" ) == 0 ){
    vtop( *rs )->u.num ++ ;
  } else if( strcmp( sigkey , "test" ) == 0 ){
    struct tval *options = vshift( vargs );
    struct tval *xcmp = klookup( "cmp" , options ); if( xcmp ){
      struct tval *vcmp = 0 ; evl( xcmp , cxs , vargs , &vcmp );
      struct tval *xeq = klookup( "eq" , options );
      if( xeq && vtop( *rs )->u.num == vtop( vcmp )->u.num ){
	evl( xeq , cxs , vargs , nxrs );
      } // if eq
    } else
      xerr("trp: test: no cmp");
  } else if( strcmp( sigkey , "brk" ) == 0 ){
    p("here");
  } else if( strcmp( sigkey , "open" ) == 0 ){
    struct tval *options = vshift( vargs );
    struct tval *xpath; if(( xpath = klookup( "path" , options ) )){
      struct tval *vpath = 0 ; evl( xpath , cxs , vargs , &vpath );
      struct tval *xoflag; if(( xoflag = klookup( "oflag" , options ) )){
	struct tval *voflag = 0 ; evl( xoflag , cxs , vargs , &voflag );
	struct tval *vnum;
	( vnum  = var( NUM ) )->u.num =
	  open( vpop( vpath )->u.str , vpop( voflag )->u.num );
	vfree( voflag );

	vpush( vnum , nxrs ); // result
      } // if oflag
      vfree( vpath );
    } // if xpath
  } else { // trp lookup
    struct tval *xchain; if(( xchain = klookup( sigkey , vtop( *rs ) ) )){
      vunshift( vtop( *rs ) , &cxs ); // left to right
      evl( xchain , cxs , vargs , nxrs );
      vshift( cxs ); // right to left
    } else {
      xerr("trp: no code");
    } // if code
  } // if builtins
} // trp
void evl
( const struct tval *xchain ,
 struct tval *cxs , struct tval *vargs , struct tval **rs ){
  if( xchain ){
    if( xchain->t != VS ) xerr("evl: unexp typ");
    for( int i = 0; i <= xchain->u.v.top; i ++ ){
      struct tval *tok = xchain->u.v.buf[ i ]->x ;
      switch( tok->t ){
      case VS : // [ array ]
	{ struct tval *rarray = 0; // prepar results array
	  for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	    vpush( 0 , &rarray );
	  } // for

	  for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	    evl( tok->u.v.buf[ i ] , cxs , vargs ,
		 &rarray->u.v.buf[ i ] );
	  } // for
	    
	  if( *rs ) vpop( *rs );
	  vpush( rarray , rs ); // pass back results
	} break;
      case NUM: // 123
	{ struct tval *num; ( num = var( NUM ) )->u.num = tok->u.num ;
	  vpush( num , rs );
	} break;

      case XS: // { obj }
	{ struct tval *kvals = 0;
	  for( int i = 0; i <= tok->u.x.top; i ++ ){
	    struct tkeytok *ktok = tok->u.x.buf[ i ];

	    // todo: dup val
	    keypush( keyvar( ktok->key , vdup( ktok->val ) ) , &kvals );
	  } // for
	  vpush( kvals, rs );
	} break;
      case KS: // { obj }
	{ struct tval *kvals = 0;
	  for( int i = 0; i <= tok->u.k.top; i ++ ){
	    struct tkeyval *kval = tok->u.k.buf[ i ];
	    keypush( keyvar( kval->key , kval->val ) , &kvals );
	  } // for
	  vpush( kvals, rs );
	} break;

      case STR: // "c-string"
	xerr("evl: STR not yet");
	break;
      case TOK: // "c-string" ?
	// todo: ought do lookup, like nam ? - _arg[0].fmt base on context
	// todo: ought have any "tok" in evl - no ?
	// todo: do custom typ - yes ?
	{ struct tval *v;
	  if( strcmp( tok->u.str , "_arg" ) == 0 ){ // reservd
	    v = vdup( vargs ); // malloc
	  } else {
	    ( v = var( STR ) )->u.str = strdup( tok->u.str ); // malloc
	    // todo: escap strings here ?
	    char *pc = v->u.str; while( *pc ){
	      if( *pc == '\\' ){
		char *nx = pc + 1;
		if ( strchr( "bn\\\'\"" , *nx ) ){
		  *pc = escap( *nx );
		  while(( nx[ 0 ] = nx[ 1 ] )) nx ++ ;
		} else if( *nx >= '0' && *nx <= '7' ){
		  xerr("evl: octal not yet\n");
		} else if( *nx == 'x' ){
		  xerr("evl: hex not yet\n");
		} // if escap char
	      } // if backslash
	      pc ++ ;
	    } // while
	  } // if _arg
	  vpush( v , rs );
	} break;
      case KVAL: // func()
	xerr("evl: typ not yet");
	break;
      case KTOK: // func( v , ... ) or nam[ a-index ]
	{ struct tval *param; // required to differ between array and function
	  if(!( param = tok->u.ktok.val ))
	    xerr("evl: no param");
	  struct tval *rsparam = 0;
	  if( param->t == AS || param->t == VS ){
	    for( int i = 0 ; i <= param->u.v.top ; i ++ ){
	      evl( param->u.v.buf[ i ] , cxs , vargs , &rsparam );
	    } // for
	  } else xerr("evl: unexp param typ");

	  char *key = tok->u.ktok.key;	 
	  struct tval *rsnx = 0;
	  switch( param->t ){
	  case AS:
	    if( key ){ // key[ elem-index ]
	      if( strcmp( key , "_arg" ) == 0 ){ // reservd
		if( rsparam->t != VS )
		  xerr("evl: unexp key[ elem-index typ ]");
		for( int i = 0; i <= rsparam->u.v.top; i ++ ){ // do lookups
		  vpush( arrlookup( rsparam->u.v.buf[ i ] , vargs ) , &rsnx );
		} // for
	      } else {
		xerr("eval: key[ elem-index ] not yet");
	      }
	    } else { // [ elem , ... ]
	      xerr("eval: [ elem , ... ] not yet");
	    } // if key
	    break;
	  case VS:
	    if( key ){
	      if( strcmp( key , "loop" ) == 0 ){
		struct tval *loopsig; if(( loopsig = vshift( rsparam ) )){
		  if( loopsig->t != STR )
		    xerr("evl: unexp loopsig typ");
		  int i = 0; while( 1 ){
		    trp( loopsig->u.str , cxs , rsparam , i ++ , rs , &rsnx );
		  }
		  vfree( loopsig );
		} else xerr("evl: no loopsig");
	      } else {
		trp( key , cxs , rsparam , 0 , rs , &rsnx );
	      } // if loop
	    } else {
	      xerr("evl: param w/o key not yet");
	    } // if key
	    break;
	  default:
	    xerr("evl: unexp param typ");
	  } // switch param->t
	  if( rsnx ){ // have next result ?
	    if( *rs ) // freen previous result now ?
	      vfree( vpop( *rs ) );
	    vpush( vpop( rsnx ) , rs );
	    vfree( rsnx );
	  }
	  vfree( param ); vfree( rsparam );
	} break; // KTOK
      case NAM: // plain nam
	if( strcmp( tok->u.str , "sys" ) == 0 ){ // rservd
	  static struct tval *syscx = 0; if( !syscx ){ // persist in memory
	    keypush( keyvar( "_sys" , 0 ) , &syscx ); // todo: temp placeholder ?
	  }
	  vpush( syscx , rs );
	} else if( strcmp( tok->u.str , "_arg" ) == 0 ){ // rservd
	  vpush( vargs , rs );
	} else {
	  struct tval *val ; if(( val = scoplookup( tok , cxs ) )){
	    vpush( val , rs );
	  } else xerr("evl: nam not found");
	} // if
	break; // NAM
      default:
	xerr("evl: unexp tok typ");
	break;
      } // switch tok->t
    } // for
  } // if xchain
}// evl
struct tval *parse( char *buf ){
  char *left = trim( buf );
  struct tval *xchain = 0; while ( *left ){
    char *right = gettok ( left , '.' );
    vpush( vtok( trim ( left ) ) , &xchain );
    left = right ;
  } // while
  for( int i = 0 ; xchain && i <= xchain->u.v.top ; i ++ ){
    left = xchain->u.v.buf[ i ]->u.str ;
    char *end = left + strlen ( left ) - 1 ;
    struct tval **x = &xchain->u.v.buf[ i ]->x ;
    switch( *end ){
    case '"' : // "c-string"
      *end = 0 ;
      *x = vtok( left + 1 );
      break;
    case ']' : // [ elem , ... ] or nam[ elem-index ]
    case ')' : // ( exp ) or func( arg , ... ) ?
      { int issquar = *end == ']' ; *end = 0 ;
	char *elem = gettok( left , issquar ? '[' : '(' );
	char *key = trim( left );
	int ( *mypush )() = issquar ? apush : vpush ;
	// using elems typ to determin between array and function ?
	struct tval *elems = var( issquar ? AS : VS ); while( *elem ){
	  char *right = gettok( elem , ',' );
	  mypush( parse( elem ) , &elems );
	  elem = right;
	} // while
	*x = strlen( key ) ? vkeytokvar( key , elems ) : elems ;
      } break;
    case '}' : // object pairs { key : val , ... }
      *end = 0 ;
      struct tval *toktmp = 0;
      left = left + 1 ; while( *left ){
	char *right = gettok ( left , ',' );
	tokpush( trim( left ) , &toktmp );
	left = right;
      } // while
      for( int i = 0; toktmp && i <= toktmp->u.s.top; i ++ ){
	left = toktmp->u.s.buf[ i ];
	char *right = gettok ( left , ':' );
	keytokpush( keytokvar( trim( left ) , parse( right ) ), x );
      } // while
      break;
    default:
      if( *end >= '!' && *end <= '~' ){
	if( *left >= '0' && *left <= '9' ){
	  ( *x = var( NUM ) )->u.num =
	    strtol( left , 0 , strncasecmp( left , "0x" , 2 ) ? 10 : 16 );
	} else {
	  *x = vtok( left );
	} // if num
      } else {
	xerr("parse: not yet");
      } // if printable
      break;
    } // switch *end
  } // for
  return xchain;
} // parse
void dmp( struct tval * );
void keydmp( struct tkeyval *kval ){
  p("%s: ", kval->key );
  struct tval *v = kval->val; dmp( v && v->x ? v->x : v );
} // keydmp
void keytokdmp( struct tkeytok *ktok ){
  p("%s: ", ktok->key );
  struct tval *v = ktok->val; dmp( v && v->x ? v->x : v );
} // ktokdmp
void dmp( struct tval *v ){
  if( v ){
    switch( v->t ){
    case NUM :
      p("%i", v->u.num);
      break;
    case STR:
    case TOK:
      p("\"%s\"", v->u.str);
      break;
    case NAM:
      p("%s", v->u.str);
      break;
    case STRS:
    case TOKS:
      p("[ ");
      for( int i = 0 ; i <= v->u.s.top ; i ++ ){
	if( i ) p(", "); p("\"%s\"", v->u.s.buf[ i ] );
      }
      p(" ]");
      break;
    case AS:
    case VS:
      { int issquar = v->t == AS;
	p( issquar ? "[ " : "( " );
	for( int i = 0 ; i <= v->u.v.top ; i ++ ){
	  if( i ) p(", "); {
	    struct tval *w = v->u.v.buf[ i ];
	    dmp( w && w->x ? w->x : w );
	  }
	} p( issquar ? " ]" : " )" );
      } break;
    case KVAL:
      keydmp( &v->u.kval );
      break;
    case KTOK:
      keytokdmp( &v->u.ktok );
      break;
    case KS:
      p("{ "); for( int i = 0 ; i <= v->u.k.top ; i ++ ){
	if( i ) p(", "); keydmp( v->u.k.buf[ i ] );
      } p(" }");
      break;
    case XS:
      p("{ "); for( int i = 0 ; i <= v->u.x.top ; i ++ ){
	if( i ) p(", "); keytokdmp( v->u.x.buf[ i ] );
      } p(" }");
      break;
    default:
      xerr("dmp: unexp typ");
    } // switch v->t
  } else p("(nul)");
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
  } else xerr("could not open file");
  
  struct tval *xchain = parse( buf );
  p("xchain:"); dmp( xchain ); p("\n");

  struct tval *argvals = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    tokpush( argv [ i ] , &argvals );
  } p("argvals:"); dmp( argvals ); p("\n");

  struct tval *rs = 0; evl( xchain , 0 , argvals , &rs );
  p("rs:"); dmp( rs ); p("\n");

  vfree( rs );
  vfree( argvals );
  vfree( xchain );
  free( buf );
  p("main: end\n");
} // main
