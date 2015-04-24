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
  while( *str == ' ' || *str == '\t' || *str == '\n' ) str ++ ;
  if( *str == 0 ) return str; // all spaces ?
  char *end = str + strlen( str ) - 1; // trim trailn space
  while
    ( str < end &&
      ( *end == ' ' || *end == '\t' || *end == '\n' )
      ) end--;
  *( end + 1 ) = 0; // terminatn
  return str;
} // trim
char *trimbrac( char *str ){
  str = trim( str );
  if( *str != '{' )
    xerr("trimbrac: no leadn {");
  str ++;
  if( *str == 0 )
    return str;
  char *end = str + strlen( str ) - 1 ;
  if( *end != '}')
    xerr("trimbrac: no trailn }");
  *end = 0;
  return str;
} // trimbrac
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
char *dquot( char *str ){
  if( *str == '"' ){ // string literal
    char *pc = str + 1;
    if( *pc == 0 ) return str;
    char *end = pc + strlen( pc ) - 1;
    if( *end != '"' )
      xerr("escap: string literal: no end dquot");
    *end = 0;
    while( *pc ){
      if( *pc == '\\' ){
	char *nx = pc + 1;
	if ( strchr( "bn\\\'\"" , *nx ) ){
	  *pc = cescap( *nx );
	  while(( nx[ 0 ] = nx[ 1 ] )) nx ++ ; // shift left
	} else if( *nx >= '0' && *nx <= '7' ){
	  xerr("evl: octal not yet\n");
	} else if( *nx == 'x' ){
	  xerr("evl: hex not yet\n");
	} // if escap char
      } // if backslash
      pc ++;
    } // while *pc
    return str + 1;
  } // if dquot
  return str;
} // dquot

typedef struct tval *tfn( const struct tval * , struct tval *);

struct tval {
  //  struct tval *x ; // child parse tree
  unsigned long flg;
#define FBRK 1
  enum tval_t {
    // TOKs point to file buf, so no string malloc, for parse
    NUL = 0,
    NUM, // long
    STR, // doubl quotd "string" literal or plain sym
    FN, // pfn
    KVAL, // { key : val } w/wo malloc
    VS,AS // ( paren ), [ array ]
  } t ;
  union {
    long num ; // NUM
    char *str ; // STR/NAM
    tfn *fn;
    struct tkeyval { char *key; struct tval *val;} kval; // KVAL
    struct tvs { int top; size_t sz; struct tval **buf;} v; // VS/AS
  } u ;
}; // tval
struct tval *evl( const struct tval *, const struct tval *, struct tval * );
struct tval *vdup( const struct tval * );

struct tval *var( int t ){
  struct tval *rt = xcalloc( 1 , sizeof( struct tval ) );
  switch( rt->t = t ){
  case NUM:
  case STR:
  case FN:
  case KVAL:
    break;
  case VS: case AS:
    rt->u.v.top = -1 ; break;
  default:
    xerr("var: unexp typ");
  } // switch
  return rt;
} // var

struct tval *vnum( long num ){
  struct tval *rt; ( rt = var( NUM ) )->u.num = num;
  return rt;
} // vstr
struct tval *vstr( const char *str ){
  struct tval *rt; ( rt = var( STR ) )->u.str = strdup( str );
  return rt;
} // vstr
struct tval *vfn( tfn *fn ){
  struct tval *rt; ( rt = var( FN ) )->u.fn = fn; // not freed - compild in ?
  return rt;
} // vstr
struct tval *vkeyvar( const char *key , struct tval *val ){
  struct tval *rt = var( KVAL );
  rt->u.kval.key = strdup( key );
  rt->u.kval.val = val; // freed, caller responsible for mallocn ?
  return rt;
} // vkeyvar

int _push( enum tval_t typ , struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != typ )
      xerr("_push: unexp typ" );
  } else *cx = var( typ );
  if( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf = realloc 
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tval * ) );
  } (*cx)->u.v.buf[ (*cx)->u.v.top ] = val;	
  return (*cx)->u.v.top ; 
} // _push
int apush( struct tval *val , struct tval **cx ){
  return _push( AS , val , cx );
} // apush
int vpush( struct tval *val , struct tval **cx ){
  return _push( VS , val , cx );
} // vpush

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
    case FN:
      rt = vfn( v->u.fn );
      break;
    case KVAL:
      // val is freed, so malloc here ?
      rt = vkeyvar( v->u.kval.key , vdup( v->u.kval.val ) );
      break;
    case VS: case AS:
      {	int ( *mypush )( struct tval * , struct tval **) = v->t == AS ?
	  apush : vpush;
	for( int i = 0; i <= v->u.v.top; i ++ ){
	  mypush( vdup( v->u.v.buf[ i ] ) , &rt );
	}
      } break;
    default:
      xerr("vdup: unexp typ");
    } // switch v->t
  } // if v
  return rt;
} // vdup
void vfree( struct tval *v ){
  if( v == 0 ) return;
  switch( v->t ){
  case NUM:
  case FN:
    break;
  case STR:
    free( v->u.str );
    v->u.str = 0;
    break;
  case KVAL:
    free( v->u.kval.key );
    vfree( v->u.kval.val ); // mallocd ?
    v->u.kval.key = 0;
    v->u.kval.val = 0;
    break;
  case VS: // ( paren )
  case AS: // [ array ]
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
    break;
  } free( v ); 
} // vfree

struct tval *vpop( struct tval *cx ){
  if( !cx )
    xerr("vpop: nul cx");
  if( cx->t != VS )
    xerr("vpop: unexp typ");
  if( cx->u.v.top < 0 ){
    p("vpop: empty\n");
  } else {
    return cx->u.v.buf[ cx->u.v.top -- ];
  } return 0;
} // vpop

int vunshift( struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != VS )
      xerr("vunshift: unexp typ");
  } else *cx = var( VS );
  if ( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){ // need more space ?
    (*cx)->u.v.buf = realloc
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tval *) );
  } for( int i = (*cx)->u.v.top ; i > 0 ; i -- ){
    (*cx)->u.v.buf[ i ] = (*cx)->u.v.buf[ i - 1 ];
  } (*cx)->u.v.buf[ 0 ] = val;  // on left ?
  return (*cx)->u.v.top;
} // vunshift
struct tval *vshift( struct tval *cx ){
  if( !cx )
    xerr("vshift: nul cx");
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
  } return 0;
} // vshift

struct tval *klookup( const char *key , const struct tval *cx ){
  if( cx->t != VS )
    xerr("klookup: unexp cx typ");
  for( int i = 0 ; i <= cx->u.v.top ; i ++ ){ // bottom to top ?
    struct tval *v = cx->u.v.buf[ i ];
    if( v && v->t == KVAL && strcmp( v->u.kval.key , key ) == 0 ){
      return v->u.kval.val ;
    } // if
  } return 0;
} // klookup
struct tval *arrlookup( const struct tval *index , const struct tval *cxs ){
  if( cxs->t != VS) xerr("arrlookup: unexp cxs typ");
  long num; switch( index->t ){
  case NUM :
    num = index->u.num;
    if( num < 0 ) xerr("arrlookup: negative index");
    if( num > cxs->u.v.top ) xerr("arrlookup: index too big");
    return cxs->u.v.buf[ num ];
    break;
  default:
    xerr("arrlookup: unexp index typ");
  } // switch
  return 0 ;
} // arrlookup
const struct tval *fnlookup( const struct tval *sym , const struct tval *cx ){
  if( sym->t != KVAL )
    xerr("fnlookup: unexp sym typ");
  if( cx->t != VS )
    xerr("fnlookup: unexp cx typ");
  for( int i = 0; i <= cx->u.v.top; i ++ ){
    struct tval *kval = cx->u.v.buf[ i ];
    if( kval->t != KVAL )
      xerr("fnlookup: unexp cx buf typ");;
    if( kval->u.kval.val->t != FN )
      xerr("fnlookup: unexp cx kval val typ");
    if( strcmp( sym->u.kval.key , kval->u.kval.key ) == 0 ){
      return kval;
    } // if
  } return 0;
} // fnlookup

struct tval *g_builtins = 0; // need to initialz

const struct tval *symlookup
(
 const struct tval *sym ,
 const struct tval *vargs ,
 const struct tval *cxs
 ){
  if( cxs->t != VS)
    xerr("symlookup: unexp cxs typ");
  switch( sym->t ){
  case KVAL:
    return fnlookup( sym , g_builtins );

  case STR:
    if( strcmp( sym->u.str , "sys" ) == 0 ){ // rservd
      static struct tval *syscx = 0; if( !syscx ){
	// tbd: temp placeholder, persist in memory ?
	vpush( vkeyvar( "_sys" , 0 ) , &syscx );
      } return syscx;
    }
    if( strcmp( sym->u.str , "_arg" ) == 0 ){ // reservd
      return vargs;
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
/*     struct tval *options = vshift( vargs ); */
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

struct tbuiltin {
  char *key;
  tfn *fn;
} g_builtinbuf[] = {
  { "p", builtin_p },
  { "incr", builtin_incr },
  { "test", builtin_test },
  { "brk", builtin_brk }
}; // g_builtinbuf

struct tval *evl
( const struct tval *vchain ,
  const struct tval *vargs ,
  struct tval *cxs
  ){
  if( vchain == 0 ){
    return 0;
  }
  struct tval *result = 0 , *last_result = 0;
  if( vchain->t == KVAL ){
    struct tval *val = vchain->u.kval.val;
    if( val->t == FN ){
      result = val->u.fn( vargs , cxs );
    } else {
      xerr("evl: unexp kval val typ");
    }
  } else {
    if( vchain->t != VS )
      xerr("evl: unexp typ");
    for( int i = 0; i <= vchain->u.v.top; i ++ ){
      struct tval *tok; switch( ( tok = vchain->u.v.buf[ i ] )->t ){
      case NUM: // 123
      case VS: // { obj }
	result = vdup( tok );
	break;
      case STR: // "c-string"
	// todo: sym lookup needs to bind 'the' value
	// todo: code value is different ? needs to execute each time ?
	result = ( *tok->u.str == '"' ?
		   vstr( dquot( tok->u.str ) ) :
		   evl( symlookup( tok , vargs , cxs ) , vargs , cxs )
		   );
	break;
      case AS : // [ array ]
	for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	  apush( evl( tok->u.v.buf[ i ] , vargs , cxs ) , &result );
	} // for
	break;
      case KVAL: // func( v , ... ) or sym[ a-index ]
	{ 
	  int ( *mypush )( struct tval * , struct tval ** ) = 0;
	  struct tval *parms_result = 0;
	  struct tval *parms; if(( parms = tok->u.kval.val )){
	    if(!( parms->t == AS || parms->t == VS )) xerr("evl: KVAL unexp parms typ");
	    mypush = parms->t == AS ? apush : vpush;
	    for( int i = 0 ; i <= parms->u.v.top ; i ++ ){ // eval params
	      mypush( evl( parms->u.v.buf[ i ] , vargs , cxs ) , &parms_result );
	    } // for
	  } // if parms
	  if( parms_result && parms_result->t == AS ){ // sym [ indexes ] - do lookups
	    const struct tval *myobj = symlookup( tok , vargs , cxs );
	    for( int i = 0; i <= parms_result->u.v.top; i ++ ){ // do lookups
	      mypush( vdup( arrlookup( parms_result->u.v.buf[ i ] , myobj ) ) , &result );
	    } // for
	  } else { // function call
	    const struct tval *evlob = 0 , *loopob = 0;
	    if( strcmp( tok->u.kval.key , "loop" ) == 0 ){ // reservd
	      struct tval *loopsig; if(( loopsig = vshift( parms_result ) )){
		if(!( loopob = symlookup( loopsig , vargs , cxs ) )){
		  xerr("evl: no loopob");
		} vfree( loopsig ); loopsig = 0;
	      } else {
		xerr("evl: loop: no sig ");
	      } // if loopsig
	    } else {
	      if(!( evlob = symlookup( tok , vargs , cxs ) )){
		xerr("evl: no evlob");
	      }
	    } // loop
	    while( 1 ){
	      result = evl( loopob ? loopob : evlob , parms_result , cxs );
	      if( !loopob || ( result && result->flg & FBRK ) ){
		break;
	      } // if break
	      if( result ){ // drop last ?
		vfree( result ); result = 0;
	      }
	    } // while
	  } // if array lookup
	  vfree( parms_result );
	} break; // case KVAL
      default:
	xerr("evl: unexp tok typ");
	break;
      } // switch tok->t
      if( result ){ // have result ?
	if( last_result ){ // drop last in context ?
	  vfree( vshift( cxs ) ); last_result = 0;
	}
	// last in chain is new context
	vunshift( last_result = result , &cxs ); result = 0;
      } // if result
    } // for vchain
  } // if vchain->t KVAL

  if( last_result ){ // remove last result from context ?
    vshift( cxs );
  } return last_result;
}// evl
struct tval *parse( char *buf ){
  struct tval *rt = 0;
  char *xchain = trim( buf );

  if( strncmp( xchain , "function", strlen( "function" ) ) == 0 ){
    
    char *xargs = gettok( xchain , '(' );
    char *xbody = trimbrac( gettok( xargs , ')' ) );
    struct tval *args = 0; while( *xargs ){
      char *right = gettok( xargs , ',' );
      vpush( parse( xargs ) , &args );
      xargs = trim( right );
    } // while
    struct tval *body = 0; while( *xbody ){
      char *right = gettok( xbody , ';' );
      vpush( parse( xbody ) , &body );
      xbody = trim( right );
    } // while
    struct tval *fn = 0;
    vpush( args , &fn );
    vpush( body , &fn );
    vpush( vkeyvar( "function" , fn ) , &rt );

  } else {
    // dotted xchain
    struct tval *tmpchain = 0; while ( *xchain ){
      char *right = gettok ( xchain , '.' );
      vpush( vstr( trim ( xchain ) ) , &tmpchain );
      xchain = trim( right );
    } // while

    for( int i = 0 ; tmpchain && i <= tmpchain->u.v.top ; i ++ ){
      char *xlink = tmpchain->u.v.buf[ i ]->u.str ;
      char *end = xlink + strlen ( xlink ) - 1 ;
      switch( *end ){
      case '"' : // "c-string"
	vpush( vstr( xlink ) , &rt );
	break;
      case ']' : // [ elem , ... ] or sym[ elem-index ]
      case ')' : // ( exp ) or func( arg , ... ) ?
	{ int issquar = *end == ']' ; *end = 0 ;
	  char *elem = gettok( xlink , issquar ? '[' : '(' );
	  char *key = trim( xlink );
	  int ( *mypush )() = issquar ? apush : vpush ;
	  // using elems typ to determin between array and function ?
	  struct tval *elems = 0; while( *elem ){
	    char *right = gettok( elem , ',' );
	    mypush( parse( elem ) , &elems );
	    elem = right;
	  } // while
	  vpush( strlen( key ) ? vkeyvar( key , elems ) : elems , &rt );
	} break;
      case '}' : // object pairs { key : val , ... }
	*end = 0 ;
	xlink ++ ;
	struct tval *toktmp = 0; while( *xlink ){
	  char *right = gettok ( xlink , ',' );
	  vpush( vstr( trim( xlink ) ) , &toktmp );
	  xlink = right;
	} // while
	struct tval *myobj = 0; for( int i = 0; toktmp && i <= toktmp->u.v.top; i ++ ){
	  xlink = toktmp->u.v.buf[ i ]->u.str;
	  char *right = gettok ( xlink , ':' );
	  vpush( vkeyvar( trim( xlink ) , parse( right ) ), &myobj );
	} // while
	vpush( myobj , &rt );
	vfree( toktmp );
	break;
      default:
	if( *end >= '!' && *end <= '~' ){
	  if( *xlink >= '0' && *xlink <= '9' ){ // num
	    vpush( vnum( strtol( xlink , 0 ,
				 strncasecmp( xlink , "0x" , 2 ) ? 10 : 16
				 ) ) , &rt );
	  } else { // plain sym
	    vpush( vstr( xlink ) , &rt );
	  } // if num
	} else {
	  xerr("parse: not yet");
	} // if printable
	break;
      } // switch *end
    } // for
    vfree( tmpchain );
  }
  return rt;
} // parse
void dmp( struct tval *v ){
  if( v ){
    switch( v->t ){
    case NUM :
      p("%i", v->u.num);
      break;
    case STR:
      p("%s", v->u.str);
      break;
    case AS:
    case VS:
      { int isarray = v->t == AS ;
	if( v->u.v.top >= 0 ){
	  p( "%c " , v->u.v.buf[ 0 ]->t == KVAL ? '{' : isarray ? '[' : '(' );
	  for( int i = 0 ; i <= v->u.v.top ; i ++ ){
	    if( i ) p(", ");
	    dmp( v->u.v.buf[ i ] );
	  }
	  p( " %c" , v->u.v.buf[ 0 ]->t == KVAL ? '}' : isarray ? ']' : ')' );
	} else {
	  p("(nul)");
	}
      } break;
    case KVAL:
      p("%s: ", v->u.kval.key );
      dmp( v->u.kval.val );
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

  // init builtin stack
  for( int i = 0; i < sizeof( g_builtinbuf ) / sizeof( *g_builtinbuf ); i ++ ){
    struct tbuiltin *kfn = &g_builtinbuf[ i ];
    vpush( vkeyvar( kfn->key , vfn( kfn->fn ) ) , &g_builtins );
  }
  
  struct tval *vchain = parse( buf );
  p("vchain:"); dmp( vchain ); p("\n");

  struct tval *argvals = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    apush( vstr( argv [ i ] ) , &argvals );
  } p("argvals:"); dmp( argvals ); p("\n");

  struct tval *rs = evl( vchain , argvals , 0 );
  p("rs:"); dmp( rs ); p("\n");

  vfree( rs );
  vfree( argvals );
  vfree( vchain );
  free( buf );
  p("main: end\n");
} // main
