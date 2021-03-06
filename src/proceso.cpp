#include "proceso.h"

#include <cstdio>

proceso::proceso( bool dbg )
{
    DEBUG = dbg;
    RECORD = false;
    frame_num = 0;
    for( int i = 0; i < 3; ++i )
        for( int j = 0; j < 3; ++j )
            kernel[ 0 ][ ( i * 3 ) + j ] = i == 1 ? 2.0 : -1.0;
    for( int i = 0; i < 3; ++i )
        for( int j = 0; j < 3; ++j )
            kernel[ 1 ][ ( i * 3 ) + j ] = i + j == 2 ? 2.0 : -1.0;
    for( int i = 0; i < 3; ++i )
        for( int j = 0; j < 3; ++j )
            kernel[ 2 ][ ( i * 3 ) + j ] = j == 1 ? 2.0 : -1.0;
    for( int i = 0; i < 3; ++i )
        for( int j = 0; j < 3; ++j )
            kernel[ 3 ][ ( i * 3 ) + j ] = i == j ? 2.0 : -1.0;
}

bool proceso::procesarImagen( char *path )
{
    frame f( path );
    if( f.valido() )
        return false;
    if( procesarImagen( f ) )
        for( char c = waitKey( 100 ); true; c = waitKey( 100 ) )
            if( c == 'q' )
                return true;
    return false;
}

bool proceso::procesarImagen( frame &f )
{
    if( f.valido() )
        return false;
    /*
        Aqui se realiza el proceso a la imagen
    */
    f.mostrarImagen( "Original" );
    /*
        Punto A.
    */
    frame E[ 4 ];
    convolucion( f, E );
    if( DEBUG )
    {
        std::string num[] = { "0", "45", "90", "135" };
        for( int i = 0; i < 4; ++i )
            E[ i ].mostrarImagen( "Convolucion " + num[ i ] );
    }

    frame edge_strong_90bw;
    threshold( E[ 2 ].imagen(), edge_strong_90bw.imagen(), 20.0, 0xFF, THRESH_BINARY | THRESH_OTSU );
    if( DEBUG )
	    edge_strong_90bw.mostrarImagen( "Edge strong 90 bw" );

    frame dilated;
    dilate( edge_strong_90bw.imagen(), dilated.imagen(), getStructuringElement( MORPH_RECT, Size( 2, 3 ) ) );
    if( DEBUG )
        dilated.mostrarImagen( "Dilated" );

    frame closed;/* TODO: el tamano mencionado en el documento causa error. */
    morphologyEx( dilated.imagen(), closed.imagen(), MORPH_CLOSE, getStructuringElement( MORPH_RECT, Size( 10, 1 ) ) );
    if( DEBUG )
        closed.mostrarImagen( "Closed" );

    frame edge_w_tresh_90( dilated.imagen() - closed.imagen() );
    edge_w_tresh_90.imagen().mul( E[ 2 ].imagen() );

    frame edge_weak_90;
    threshold( edge_w_tresh_90.imagen(), edge_weak_90.imagen(), 20.0, 0xFF, THRESH_BINARY | THRESH_OTSU );


    if( DEBUG )
        edge_weak_90.mostrarImagen( "Edge weak 90" );

    frame edge_bw_90( edge_strong_90bw.imagen() + edge_weak_90.imagen() );
    if( DEBUG )
        edge_bw_90.mostrarImagen( "Edge bw 90" );

    /*
        TODO:
            Hasta aqui el resultado se ve bien. OpenCV no implementa un operador
            thinning. Usando erode no se obtienen buenos resultado. Mi implementacion
            del operador thinning no es muy buena.
            Todavia no se usa este resultado.
    */
    frame prethin;
    dilate( edge_bw_90.imagen(), prethin.imagen(), getStructuringElement( MORPH_RECT, Size( 0, 4 ) ) );
    frame thinned = prethin.thinning(  );
    //erode( edge_bw_90.imagen(), thinned.imagen(), getStructuringElement( MORPH_RECT, Size( 2, 3 ) ) );
    if( DEBUG )
        thinned.mostrarImagen( "Thinned" );
    frame short_ed_bw_90 = edge_bw_90.labeling( 15, 70 );
    //if( DEBUG )
        short_ed_bw_90.mostrarImagen( "Short edges" );

    frame candidate;
    dilate( short_ed_bw_90.imagen(), candidate.imagen(), getStructuringElement( MORPH_RECT, Size( 3, 6 ) ) );

    if( DEBUG )
        candidate.mostrarImagen( "Candidatos" );

    frame sums;
    add( E[ 0 ].imagen(), E[ 1 ].imagen(), sums.imagen() );
    add( sums.imagen(), E[ 2 ].imagen(), sums.imagen() );
    add( sums.imagen(), E[ 3 ].imagen(), sums.imagen() );
    frame refined( candidate.imagen().mul( sums.imagen() ) );
    if( DEBUG )
        refined.mostrarImagen( "Refinada" );

    frame fmap( refined.imagen() );
    int c = 1; /* window size */
    for( int i = 0, h = refined.imageSize().height; i < h; ++i )
        for( int j = 0, w = refined.imageSize().width; j < w; ++j )
        {
            unsigned int suma = 0;
            for( int m = -c; m <= c; ++m )
                for( int n = -c; n <= c; ++n )
                    if( i + m >= 0 && i + m < h && j + n >= 0 && j + n < w )
                        suma += refined.dataAt( i + m, j + n );
            suma *= weight( refined, i, j );
            suma = suma / 36;
            fmap.setData( i, j, suma );
        }
    fmap.mostrarImagen( "fmap" );

    /**/
    /*frame idea, idea_label, anded, anded_short;
    morphologyEx( refined.imagen(), idea.imagen(), MORPH_CLOSE, getStructuringElement( MORPH_RECT, Size( 10, 1 ) ) );
    if( DEBUG )
        idea.mostrarImagen( "Idea" );

    idea_label = idea.labeling( 3, 999999 );
    if( DEBUG )
        idea_label.mostrarImagen( "Idea labeled" );

    anded = idea_label & refined;
    if( DEBUG )
        anded.mostrarImagen( "Anded" );

    anded_short = idea_label & short_ed_bw_90;
    if( DEBUG )
        anded_short.mostrarImagen( "Anded short" );

    anded.drawTextHull( f );
    f.mostrarImagen( "Resultado" );*/
    /*
        Fin punto A.
    */

    return true;
}

int proceso::weight( frame &f, int j, int i )
{
    int w = 0, wl = f.imageSize().width, hl = f.imageSize().height;
    if( i < 0 || i >= w || j < 0 || j >= hl )
        return 0;
    if( f.dataAt( i, j ) != 0xff )
        return 0;
    if( i > 0 && i + 1 < wl && f.dataAt( i - 1, j ) == 0xff && f.dataAt( i + 1, j ) == 0xff )
        ++w;
    if( j > 0 && j + 1 < hl && f.dataAt( i, j - 1 ) == 0xff && f.dataAt( i, j + 1 ) == 0xff )
        ++w;
    if( i > 0 && i + 1 < wl && j + 1 < hl && j > 0 && f.dataAt( i - 1, j + 1 ) == 0xff && f.dataAt( i + 1, j - 1 ) == 0xff )
        ++w;
    if( i > 0 && i + 1 < wl && j + 1 < hl && j > 0 && f.dataAt( i - 1, j - 1 ) == 0xff && f.dataAt( i + 1, j + 1 ) == 0xff )
        ++w;
    return w;
}

void proceso::convolucion( frame &f, frame *conv )
{
    Mat bw( f.imagen().size(), CV_8UC1 ); /* imagen escala de grises */

    cvtColor( f.imagen(), bw, CV_RGB2GRAY ); /* pone el contenido de f en bw en escala de grises */

    for( int i = 0; i < 4; ++i ) /* convolucion de las cuatro imagenes */
    {
        conv[ i ] = frame( bw );
        conv[ i ].convolucion( kernel[ i ] );
    }
}

bool proceso::procesarVideo( char *path, char *save )
{
    bool s = ( save != NULL );
    if( ( !s && !captura.cargar( path == NULL ? "" : path ) ) || ( s && !captura.cargar( path == NULL ? "" : path, save ) ) )
        return false;

    Mat f;

    while( captura.proxima( f ) )
    {
        frame ft( f );
        procesarImagen( ft );
        captura.escribir( ft.imagen() );
        //capacidad de pausar
        char c = waitKey( 5 );
        if( c == 27 )
            for( char c = waitKey( 100 ); true; c = waitKey( 100 ) )
            {
                if( c == 27 )
                    break;
                else if( c == 'q' )
                    return true;
            }
        else if( c == 'q' )
            return true;
    }
    return true;
}
