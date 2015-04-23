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
  struct tval *rt; ( rt = var( FN ) )->u.fn = fn;
  return rt;
} // vstr
struct tval *vkeyvar( const char *key , struct tval *val ){
  struct tval *rt = var( KVAL );
  rt->u.kval.key = strdup( key );
  rt->u.kval.val = val;
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

/* #define PUSH( PRE , TYP , CTYP , UTYP )				\ */
/*   int PRE ## push( CTYP val , struct tval **cx ){			\ */
/*     if( *cx ){								\ */
/*       if( (*cx)->t != TYP )						\ */
/* 	xerr( #PRE "push: unexp typ" );					\ */
/*     } else *cx = var( TYP );					\ */
/*     if( ++ (*cx)->u.UTYP.top >= (*cx)->u.UTYP.sz ){			\ */
/*       (*cx)->u.UTYP.buf = realloc					\ */
/* 	( (*cx)->u.UTYP.buf , (*cx)->u.UTYP.sz += 8 * sizeof( CTYP ) );	\ */
/*     } (*cx)->u.UTYP.buf[ (*cx)->u.UTYP.top ] = val;		\ */
/*     return (*cx)->u.UTYP.top ;						\ */
/*   } */

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
      rt = vkeyvar( v->u.kval.key , v->u.kval.val );
      break;
    case VS: case AS:
      {	int ( *mypush )( struct tval * , struct tval **) =
	  v->t == AS ? apush : vpush;
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
    break;
  case STR:
    free( v->u.str );
    v->u.str = 0;
    break;
  case KVAL:
    free( v->u.kval.key );
    vfree( v->u.kval.val );
    v->u.kval.val = 0;
    v->u.kval.key = 0; // mem alloc
    break;
  case VS: // ( paren )
  case AS: // [ array ]
    for( int i = 0; i <= v->u.v.top; i ++ ){
      vfree( v->u.v.buf[ i ] ); v->u.v.buf[ i ] = 0;
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
  if( cx ){
    if( cx->t != VS )
      xerr("vpop: unexp typ");
    if( cx->u.v.top < 0 ){
      p("vpop: empty\n");
    } else {
      return cx->u.v.buf[ cx->u.v.top -- ];
    }
  } else {
    xerr("vpop: nul cx");
  } return 0;
} // vpop

int vunshift( struct tval *val , struct tval **cx ){
  if( *cx ){
    if( (*cx)->t != VS ) xerr("vunshift: unexp typ");
  } else *cx = var( VS );
  if ( ++ (*cx)->u.v.top >= (*cx)->u.v.sz ){
    (*cx)->u.v.buf = realloc
      ( (*cx)->u.v.buf , (*cx)->u.v.sz += 8 * sizeof( struct tval *) );
  } for( int i = (*cx)->u.v.top ; i > 0 ; i -- ){
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
  } else
    xerr("vshift: nul cx");
  return 0;
} // vshift

struct tval *klookup( const char *key , const struct tval *cx ){
  if( cx->t != VS ) xerr("klookup: unexp cx typ");
  for( int i = 0 ; i <= cx->u.v.top ; i ++ ){ // bottom to top ?
    struct tval *v = cx->u.v.buf[ i ];
    if( v->t == KVAL ){
      if( strcmp( v->u.kval.key , key ) == 0 ){
	return v->u.kval.val ;
      } // if
    } // if KVAL
  } // for
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

struct tval *builtin_p( const struct tval *vargs , struct tval *cxs ){
  struct tval *rt = 0;
  if( vargs->u.v.top < 0 ) xerr("builtin_p: missing options");
  struct tval *options = vargs->u.v.buf[ 0 ];
  struct tval *xfmt = klookup( "fmt" , options ); if( xfmt ){
    struct tval *vfmt = evl( xfmt , vargs , cxs );
    struct tval *xarg1 = klookup( "arg1" , options ); if( xarg1 ){
      struct tval *varg1 = evl( xarg1 , vargs , cxs );
      struct tval *xarg2 = klookup( "arg2" , options ); if( xarg2 ){
	struct tval *varg2 = evl( xarg2 , vargs , cxs );
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
  } else {
    xerr("builtin_p: fmt: no code");
  } // if xfmt
  return rt;
} // builtin_p

struct tval *builtins = 0; // need to initialz
struct tbuiltin {
  char *key;
  tfn *fn;
} builtin_buf[] = {
  { "p", builtin_p }
}; // builtin_bufs

const struct tval *fnlookup( const struct tval *sym , const struct tval *cx ){
  if( sym->t != KVAL ) xerr("fnlookup: unexp sym typ");
  if( cx->t != VS ) xerr("fnlookup: unexp cx typ");
  const struct tval *rt = 0;
  for( int i = 0; i <= cx->u.v.top; i ++ ){
    struct tval *kval;
    if(( kval = cx->u.v.buf[ i ] )->t != KVAL ) xerr("fnlookup: unexp cx buf typ");
    if( kval->u.kval.val->t != FN ) xerr("fnlookup: unexp cx kval val typ");
    if( strcmp( sym->u.kval.key , kval->u.kval.key ) == 0 ){ // check builtins
      return kval;
    }
  }
/*  else if( strcmp( sym->sigkey , "incr" ) == 0 ){ */
/*     vtop( vtop( *prvres ) )->u.num ++ ; */
/*   } else if( strcmp( sigkey , "test" ) == 0 ){ */
/*     struct tval *options = vshift( vargs ); */
/*     struct tval *xcmp = klookup( "cmp" , options ); if( xcmp ){ */
/*       struct tval *vcmp = 0 ; evl( xcmp , cxs , vargs , &vcmp ); */
/*       struct tval *xeq = klookup( "eq" , options ); */
/*       if( xeq && vtop( vtop( *prvres ) )->u.num == vtop( vcmp )->u.num ){ */
/* 	evl( xeq , cxs , vargs , nxtres ); */
/*       } // if eq */
/*     } else */
/*       xerr("trp: test: no cmp"); */
/*   } else if( strcmp( sigkey , "brk" ) == 0 ){ */
/*     p("here"); */
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

  return rt;
} // fnlookup

const struct tval *symlookup
(
 const struct tval *sym ,
 const struct tval *cxs ,
 const struct tval *vargs
 ){
  if( cxs->t != VS) xerr("symlookup: unexp cxs typ");
  switch( sym->t ){
  case KVAL:
    return fnlookup( sym , builtins );

  case STR:
    if( strcmp( sym->u.str , "sys" ) == 0 ){ // rservd
      // todo: temp placeholder, persist in memory
      static struct tval *syscx = 0; if( !syscx ){
	vpush( vkeyvar( "_sys" , 0 ) , &syscx );
      }
      return syscx;
    }
    if( strcmp( sym->u.str , "_arg" ) == 0 ){ // resrvd ?
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

struct tval *evl
( const struct tval *xchain ,
  const struct tval *vargs ,
  struct tval *cxs
  ){
  if( xchain == 0 ){
    p("evl: no xchain");
    return 0;
  }
  struct tval *result = 0 , *prev_result = 0;
  if( xchain->t == KVAL ){
    struct tval *val = xchain->u.kval.val;
    if( val->t == FN ){
      vpush( val->u.fn( vargs , cxs ) , &result );
    } else {
      xerr("evl: unexp kval val typ");
    }
  } else {
    if( xchain->t != VS )
      xerr("evl: unexp typ");
    for( int i = 0; i <= xchain->u.v.top; i ++ ){
      struct tval *tok; switch( ( tok = xchain->u.v.buf[ i ] )->t ){
      case NUM: // 123
      case VS: // { obj }
	vpush( vdup( tok ) , &result );
	break;
      case STR: // "c-string"
	vpush( *tok->u.str == '"' ?
	       vstr( dquot( tok->u.str ) ) :
	       vdup( symlookup( tok , cxs , vargs ) ) ,
	       &result );
	break;

      case AS : // [ array ]
	for( int i = 0 ; i <= tok->u.v.top ; i ++ ){
	  vpush( evl( tok->u.v.buf[ i ] , vargs , cxs ) ,
		 &result );
	} // for
	break;
      case KVAL: // func( v , ... ) or sym[ a-index ]
	{	struct tval *parms_result = 0;
	  int ( *mypush )( struct tval * , struct tval ** ) = 0;
	  struct tval *parms; if(( parms = tok->u.kval.val )){
	    if(!( parms->t == AS || parms->t == VS )) xerr("evl: KVAL unexp parms typ");
	    mypush = parms->t == AS ? apush : vpush;
	    for( int i = 0 ; i <= parms->u.v.top ; i ++ ){ // eval params
	      mypush( evl( parms->u.v.buf[ i ] , vargs , cxs ) , &parms_result );
	    } // for
	  }
	  if( parms_result && parms_result->t == AS ){ // sym [ indexes ] - do lookups
	    const struct tval *myobj = symlookup( tok , cxs , vargs );
	    for( int i = 0; i <= parms_result->u.v.top; i ++ ){ // do lookups
	      mypush( vdup( arrlookup( parms_result->u.v.buf[ i ] , myobj ) ) , &result );
	    } // for
	  } else { // function call
	    const struct tval *evlobj = 0 , *loopobj = 0;
	    if( strcmp( tok->u.kval.key , "loop" ) == 0 ){ // reservd
	      struct tval *loopsig; if(( loopsig = vshift( parms_result ) )){
		loopobj = symlookup( loopsig , cxs , vargs );
		vfree( loopsig ); loopsig = 0;
	      } else {
		xerr("evl: loop: no sig ");
	      } // if loopsig
	    } else {
	      evlobj = symlookup( tok , cxs , vargs );
	    } // loop
	    while( 1 ){
	      vpush( evl( loopobj ? loopobj : evlobj , parms_result , cxs ) , &result );
	      if( !loopobj || vtop( result )->flg & FBRK ){
		break;
	      } // if break
	      vfree( result ); result = 0; // dropn previous iterations
	    } // while
	  } // if array lookup
	} break; // case KVAL
      default:
	xerr("evl: unexp tok typ");
	break;
      } // switch tok->t
      if( result && result->u.v.top >= 0 ){
	if( prev_result ){
	  vshift( cxs ); // drop previous
	  vfree( prev_result ); prev_result = 0;
	} vunshift( vdup( prev_result = vpop( result ) ) , &cxs );
      } // if result
    } // for
  } // if xchain->t KVAL

  if( result && result->u.v.top >=0 ){
    p("evl: %i leftover results" , result->u.v.top + 1 );
  } vfree( result ); result = 0;
  if( prev_result ){
    vshift( cxs ); // drop previous context
  } return prev_result;
}// evl
struct tval *parse( char *buf ){
  struct tval *rt = 0;
  char *left = trim( buf );
  struct tval *tmpchain = 0; while ( *left ){
    char *right = gettok ( left , '.' );
    vpush( vstr( trim ( left ) ) , &tmpchain );
    left = right ;
  } // while
  for( int i = 0 ; tmpchain && i <= tmpchain->u.v.top ; i ++ ){
    left = tmpchain->u.v.buf[ i ]->u.str ;
    char *end = left + strlen ( left ) - 1 ;
    switch( *end ){
    case '"' : // "c-string"
      vpush( vstr( left ) , &rt );
      break;
    case ']' : // [ elem , ... ] or sym[ elem-index ]
    case ')' : // ( exp ) or func( arg , ... ) ?
      { int issquar = *end == ']' ; *end = 0 ;
	char *elem = gettok( left , issquar ? '[' : '(' );
	char *key = trim( left );
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
      left ++ ;
      struct tval *toktmp = 0; while( *left ){
	char *right = gettok ( left , ',' );
	vpush( vstr( trim( left ) ) , &toktmp );
	left = right;
      } // while
      struct tval *myobj = 0; for( int i = 0; toktmp && i <= toktmp->u.v.top; i ++ ){
	left = toktmp->u.v.buf[ i ]->u.str;
	char *right = gettok ( left , ':' );
	vpush( vkeyvar( trim( left ) , parse( right ) ), &myobj );
      } // while
      vfree( toktmp );
      vpush( myobj , &rt );
      break;
    default:
      if( *end >= '!' && *end <= '~' ){
	if( *left >= '0' && *left <= '9' ){ // num
	  vpush( vnum( strtol( left , 0 ,
			       strncasecmp( left , "0x" , 2 ) ? 10 : 16
			       ) ) , &rt );
	} else { // plain sym
	  vpush( vstr( left ) , &rt );
	} // if num
      } else {
	xerr("parse: not yet");
      } // if printable
      break;
    } // switch *end
  } // for
  vfree( tmpchain );
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
  for( int i = 0; i < sizeof( builtin_buf ) / sizeof( *builtin_buf ); i ++ ){
    struct tbuiltin *kfn = &builtin_buf[ i ];
    vpush( vkeyvar( kfn->key , vfn( kfn->fn ) ) , &builtins );
  }
  
  struct tval *xchain = parse( buf );
  p("xchain:"); dmp( xchain ); p("\n");

  struct tval *argvals = 0 ; for( int i = 0 ; i < argc ; i ++ ){
    apush( vstr( argv [ i ] ) , &argvals );
  } p("argvals:"); dmp( argvals ); p("\n");

  struct tval *rs = evl( xchain , argvals , 0 );
  p("rs:"); dmp( rs ); p("\n");

  vfree( rs );
  vfree( argvals );
  vfree( xchain );
  free( buf );
  p("main: end\n");
} // main
