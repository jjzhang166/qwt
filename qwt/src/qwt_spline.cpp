/* -*- mode: C++ ; c-file-style: "stroustrup" -*- *****************************
 * Qwt Widget Library
 * Copyright (C) 1997   Josef Wilgen
 * Copyright (C) 2002   Uwe Rathmann
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the Qwt License, Version 1.0
 *****************************************************************************/

#include "qwt_spline.h"
#include "qwt_spline_parametrization.h"
#include "qwt_math.h"

static inline QPointF qwtBezierPoint( const QPointF &p1,
    const QPointF &cp1, const QPointF &cp2, const QPointF &p2, double t )
{
    const double d1 = 3.0 * t;
    const double d2 = 3.0 * t * t;
    const double d3 = t * t * t;
    const double s  = 1.0 - t;

    const double x = (( s * p1.x() + d1 * cp1.x() ) * s + d2 * cp2.x() ) * s + d3 * p2.x();
    const double y = (( s * p1.y() + d1 * cp1.y() ) * s + d2 * cp2.y() ) * s + d3 * p2.y();

    return QPointF( x, y );
}

namespace QwtSplineC1P
{
    struct param
    {
        param( const QwtSplineParametrization *p ):
            parameter( p )
        {
        }

        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return parameter->valueIncrement( p1, p2 );
        }

        const QwtSplineParametrization *parameter;
    };

    struct paramY
    {
        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return QwtSplineParametrization::valueIncrementY( p1, p2 );
        }
    };

    struct paramUniform
    {
        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return QwtSplineParametrization::valueIncrementUniform( p1, p2 );
        }
    };

    struct paramCentripetal
    {
        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return QwtSplineParametrization::valueIncrementCentripetal( p1, p2 );
        }
    };

    struct paramChordal
    {
        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return QwtSplineParametrization::valueIncrementChordal( p1, p2 );
        }
    };

    struct paramManhattan
    {
        inline double operator()( const QPointF &p1, const QPointF &p2 ) const
        {
            return QwtSplineParametrization::valueIncrementManhattan( p1, p2 );
        }
    };

    class PathStore
    {
    public:
        inline void init( int size )
        {
            Q_UNUSED(size);
        }

        inline void start( double x1, double y1 )
        {
            path.moveTo( x1, y1 );
        }

        inline void addCubic( double cx1, double cy1,
            double cx2, double cy2, double x2, double y2 )
        {
            path.cubicTo( cx1, cy1, cx2, cy2, x2, y2 );
        }

        inline void end()
        {
            path.closeSubpath();
        }

        QPainterPath path;
    };

    class ControlPointsStore
    {
    public:
        inline ControlPointsStore():
            d_cp( NULL )
        {
        }

        inline void init( int size )
        {
            controlPoints.resize( size );
            d_cp = controlPoints.data();
        }   

        inline void start( double x1, double y1 )
        {   
            Q_UNUSED( x1 );
            Q_UNUSED( y1 );
        }
        
        inline void addCubic( double cx1, double cy1,
            double cx2, double cy2, double x2, double y2 )
        {
            Q_UNUSED( x2 );
            Q_UNUSED( y2 );

            QLineF &l = *d_cp++;
            l.setLine( cx1, cy1, cx2, cy2 );
        }

        inline void end()
        {
        } 

        QVector<QLineF> controlPoints;

    private:
        QLineF* d_cp;
    };
}

template< class SplineStore >
static inline SplineStore qwtSplineC1PathParamX(
    const QwtSplineC1 *spline, const QPolygonF &points )
{
    const int n = points.size();

    const QVector<double> m = spline->slopes( points );
    if ( m.size() != n )
        return SplineStore();

    const QPointF *pd = points.constData();
    const double *md = m.constData();

    SplineStore store;
    store.init( m.size() - 1 );
    store.start( pd[0].x(), pd[0].y() );

    for ( int i = 0; i < n - 1; i++ )
    {
        const double dx3 = ( pd[i+1].x() - pd[i].x() ) / 3.0;

        store.addCubic( pd[i].x() + dx3, pd[i].y() + md[i] * dx3,
            pd[i+1].x() - dx3, pd[i+1].y() - md[i+1] * dx3,
            pd[i+1].x(), pd[i+1].y() );
    }

    return store;
}

template< class SplineStore >
static inline SplineStore qwtSplineC1PathParamY(
    const QwtSplineC1 *spline, const QPolygonF &points )
{
    const int n = points.size();

    QPolygonF pointsFlipped( n );
    for ( int i = 0; i < n; i++ )
    {
        pointsFlipped[i].setX( points[i].y() );
        pointsFlipped[i].setY( points[i].x() );
    }

    const QVector<double> m = spline->slopes( pointsFlipped );
    if ( m.size() != n )
        return SplineStore();

    const QPointF *pd = pointsFlipped.constData();
    const double *md = m.constData();

    SplineStore store;
    store.init( m.size() - 1 );
    store.start( pd[0].y(), pd[0].x() );

    QVector<QLineF> lines( n );
    for ( int i = 0; i < n - 1; i++ )
    {
        const double dx3 = ( pd[i+1].x() - pd[i].x() ) / 3.0;

        store.addCubic( pd[i].y() + md[i] * dx3, pd[i].x() + dx3, 
            pd[i+1].y() - md[i+1] * dx3, pd[i+1].x() - dx3, 
            pd[i+1].y(), pd[i+1].x() );
    }

    return store;
}

template< class SplineStore, class Param >
static inline SplineStore qwtSplineC1PathParametric( 
    const QwtSplineC1 *spline, const QPolygonF &points, Param param )
{
    const int n = points.size();

    QPolygonF px, py;

    px += QPointF( 0.0, points[0].x() );
    py += QPointF( 0.0, points[0].y() );

    double t = 0.0;
    for ( int i = 1; i < n; i++ )
    {
        t += param( points[i-1], points[i] );

        px += QPointF( t, points[i].x() );
        py += QPointF( t, points[i].y() );
    }

    if ( spline->isClosing() )
    {
        t += param( points[n-1], points[0] );

        px += QPointF( t, points[0].x() );
        py += QPointF( t, points[0].y() );
    }

    const QVector<double> mx = spline->slopes( px );
    const QVector<double> my = spline->slopes( py );

    py.clear(); // we don't need it anymore

    SplineStore store;
    store.init( n - 1 );
    store.start( points[0].x(), points[0].y() );

    for ( int i = 1; i < n; i++ )
    {
#if 1
        const double t3 = param( points[i-1], points[i] ) / 3.0;
#else
        const double t3 = ( px[i].x() - px[i-1].x() ) / 3.0;
#endif

        const double cx1 = points[i-1].x() + mx[i-1] * t3;
        const double cy1 = points[i-1].y() + my[i-1] * t3;

        const double cx2 = points[i].x() - mx[i] * t3;
        const double cy2 = points[i].y() - my[i] * t3;

        store.addCubic( cx1, cy1, cx2, cy2, points[i].x(), points[i].y() );
    }

    if ( spline->isClosing() )
    {
        const double t3 = param( points[n-1], points[0] ) / 3.0;

        const double cx1 = points[n-1].x() + mx[n] * t3;
        const double cy1 = points[n-1].y() + my[n] * t3;

        const double cx2 = points[0].x() - mx[0] * t3;
        const double cy2 = points[0].y() - my[0] * t3;

        store.addCubic( cx1, cy1, cx2, cy2, points[0].x(), points[0].y() );
        store.end();
    }

    return store;
}

template< QwtSplinePolynomial toPolynomial( const QPointF &, double, const QPointF &, double ) >
static QPolygonF qwtPolygonParametric( double distance,
    const QPolygonF &points, const QVector<double> values, bool withNodes ) 
{
    QPolygonF fittedPoints;

    const QPointF *p = points.constData();
    const double *v = values.constData();
    
    fittedPoints += p[0];
    double t = distance;
    
    const int n = points.size();

    for ( int i = 0; i < n - 1; i++ )
    {
        const QPointF &p1 = p[i];
        const QPointF &p2 = p[i+1];

        const QwtSplinePolynomial polynomial = toPolynomial( p1, v[i], p2, v[i+1] );
            
        const double l = p2.x() - p1.x();
        
        while ( t < l )
        {
            fittedPoints += QPointF( p1.x() + t, p1.y() + polynomial.valueAt( t ) );
            t += distance;
        }   
        
        if ( withNodes )
        {
            if ( qFuzzyCompare( fittedPoints.last().x(), p2.x() ) )
                fittedPoints.last() = p2;
            else
                fittedPoints += p2;
        }       
        else
        {
            t -= l;
        }   
    }   
    
    return fittedPoints;
}

/*!
  \brief Constructor

  The default setting is a non closing spline with chordal parametrization

  \sa setParametrization(), setClosing()
 */
QwtSpline::QwtSpline():
    d_parametrization( new QwtSplineParametrization( QwtSplineParametrization::ParameterChordal ) ),
    d_isClosing( false )
{
}

//! Destructor
QwtSpline::~QwtSpline()
{
    delete d_parametrization;
}

/*!
  The locality of an spline interpolation identifies how many adjacent
  polynoms are affected, when changing the position of one point.

  A locality of 'n' means, that changing the coordinates of a point
  has an effect on 'n' leading and 'n' following polynoms.
  Those polynoms can be calculated from a local subpolygon.

  A value of 0 means, that the interpolation is not local and any modification
  of the polygon requires to recalculate all polynoms ( f.e cubic splines ). 

  \return Order of locality
 */
uint QwtSpline::locality() const
{
    return 0;
}

/*!
  \brief Set or clear if the interpolation is closing

  When a spline is closing the interpolation includes
  the line between the last and the first control point.

  The default setting is not closing.

  \sa isClosing(), QwtSplineC1::Periodic
 */
void QwtSpline::setClosing( bool on )
{
    d_isClosing = on;
}

/*!
  When a spline is closing the interpolation includes
  the line between the last and the first control point.

  \return True, when the spline is closing
  \sa setClosing()
 */
bool QwtSpline::isClosing() const
{
    return d_isClosing;
}

void QwtSpline::setParametrization( int type )
{
    if ( d_parametrization->type() != type )
    {
        delete d_parametrization;
        d_parametrization = new QwtSplineParametrization( type );
    }
}

void QwtSpline::setParametrization( QwtSplineParametrization *parametrization )
{
    if ( ( parametrization != NULL ) && ( d_parametrization != parametrization ) )
    {
        delete d_parametrization;
        d_parametrization = parametrization;
    }
}   

const QwtSplineParametrization *QwtSpline::parametrization() const
{
    return d_parametrization;
}

/*! \fn QVector<QLineF> bezierControlLines( const QPolygonF &points ) const

  \brief Interpolate a curve with Bezier curves

  Interpolates a polygon piecewise with cubic Bezier curves
  and returns the 2 control points of each curve as QLineF.

  \param points Control points
  \return Control points of the interpolating Bezier curves
 */

/*!
  \brief Interpolate a curve with Bezier curves

  Interpolates a polygon piecewise with cubic Bezier curves
  and returns them as QPainterPath.

  The implementation calculates the Bezier control lines first
  and converts them into painter path elements in an additional loop.
  
  \param points Control points
  \return Painter path, that can be rendered by QPainter

  \note Derived spline classes might overload painterPath() to avoid
        the extra loops for converting results into a QPainterPath

  \sa bezierControlLines()
 */
QPainterPath QwtSpline::painterPath( const QPolygonF &points ) const
{
    const int n = points.size();

    QPainterPath path;
    if ( n == 0 )
        return path;

    if ( n == 1 )
    {
        path.moveTo( points[0] );
        return path;
    }

    if ( n == 2 )
    {
        path.addPolygon( points );
        return path;
    }

    const QVector<QLineF> controlPoints = bezierControlLines( points );
    if ( controlPoints.size() < n - 1 )
        return path;

    const QPointF *p = points.constData();
    const QLineF *l = controlPoints.constData();

    path.moveTo( p[0] );
    for ( int i = 0; i < n - 1; i++ )
        path.cubicTo( l[i].p1(), l[i].p2(), p[i+1] );

    if ( controlPoints.size() >= n )
    {
        path.cubicTo( l[n-1].p1(), l[n-1].p2(), p[0] );
        path.closeSubpath();
    }

    return path;
}

/*!
  \brief Find an interpolated polygon with "equidistant" points

  When withNodes is disabled all points of the resulting polygon
  will be equidistant according to the parametrization. 

  When withNodes is enabled the resulting polygon will also include
  the control points and the interpolated points are always aligned to
  the control point before ( points[i] + i * distance ).

  The implementation calculates bezier curves first and calculates
  the interpolated points in a second run.

  \param points Control nodes of the spline
  \param distance Distance between 2 points according 
                  to the parametrization
  \param withNodes When true, also add the control 
                   nodes ( even if not being equidistant )

  \sa bezierControlLines()
 */
QPolygonF QwtSpline::equidistantPolygon( const QPolygonF &points, 
    double distance, bool withNodes ) const
{
    if ( distance <= 0.0 )
        return QPolygonF();

    const int n = points.size();
    if ( n <= 1 )
        return points;

    if ( n == 2 )
    {
        // TODO
        return points;
    }

    QPolygonF path;

    const QVector<QLineF> controlLines = bezierControlLines( points );

    if ( controlLines.size() < n - 1 )
        return path;

    path += points.first();
    double t = distance;

    const QPointF *p = points.constData();
    const QLineF *cl = controlLines.constData();

    for ( int i = 0; i < n - 1; i++ )
    {
        const double l = d_parametrization->valueIncrement( p[i], p[i+1] );

        while ( t < l )
        {
            path += qwtBezierPoint( p[i], cl[i].p1(),
                cl[i].p2(), p[i+1], t / l );

            t += distance;
        }

        if ( withNodes )
        {
            if ( qFuzzyCompare( path.last().x(), p[i+1].x() ) )
                path.last() = p[i+1];
            else
                path += p[i+1];

            t = distance;
        }
        else
        {
            t -= l;
        }
    }

    if ( isClosing() && ( controlLines.size() >= n ) )
    {
        const double l = d_parametrization->valueIncrement( p[n-1], p[0] );

        while ( t < l )
        {
            path += qwtBezierPoint( p[n-1], cl[n-1].p1(),
                cl[n-1].p2(), p[0], t / l );

            t += distance;
        }

        if ( qFuzzyCompare( path.last().x(), p[0].x() ) )
            path.last() = p[0];
        else 
            path += p[0];
    }

    return path;
}

//! Constructor
QwtSplineG1::QwtSplineG1()
{
}

//! Destructor
QwtSplineG1::~QwtSplineG1()
{
}

class QwtSplineC1::PrivateData
{
public:
    PrivateData()
    {
        boundaryCondition.type = QwtSplineC1::ParabolicRunout;
        boundaryCondition.value[0] = boundaryCondition.value[1] = 0.0;
    }

    void setBoundaryCondition( QwtSplineC1::BoundaryCondition condition, 
        double valueStart, double valueEnd )
    {
        boundaryCondition.type = condition;
        boundaryCondition.value[0] = valueStart;
        boundaryCondition.value[1] = valueEnd;
    }

    struct
    {
        QwtSplineC1::BoundaryCondition type;
        double value[2];

    } boundaryCondition;
};

QwtSplineC1::QwtSplineC1()
{
    setParametrization( QwtSplineParametrization::ParameterX );
    d_data = new PrivateData;
}

//! Destructor
QwtSplineC1::~QwtSplineC1()
{
    delete d_data;
}

void QwtSplineC1::setBoundaryConditions( BoundaryCondition condition )
{
    if ( condition >= Natural )
        d_data->setBoundaryCondition( condition, 0.0, 0.0 );
    else
        d_data->setBoundaryCondition( condition, boundaryValueBegin(), boundaryValueEnd()  );
}

QwtSplineC1::BoundaryCondition QwtSplineC1::boundaryCondition() const
{
    return d_data->boundaryCondition.type;
}

void QwtSplineC1::setBoundaryValues( double valueBegin, double valueEnd )
{
    d_data->setBoundaryCondition( boundaryCondition(), valueBegin, valueEnd );
}

double QwtSplineC1::boundaryValueBegin() const
{
    return d_data->boundaryCondition.value[0];
}

double QwtSplineC1::boundaryValueEnd() const
{
    return d_data->boundaryCondition.value[1];
}

double QwtSplineC1::slopeBegin( const QPolygonF &points, double m1, double m2 ) const
{
    const int size = points.size();
    if ( size < 2 )
        return 0.0;

    const double boundaryValue = d_data->boundaryCondition.value[0];

    if ( boundaryCondition() == QwtSplineC1::Clamped )
        return boundaryValue;

    const double dx = points[1].x() - points[0].x();
    const double dy = points[1].y() - points[0].y();

    if ( boundaryCondition() == QwtSplineC1::LinearRunout )
    {
        const double s = dy / dx;
        return s - boundaryValue * ( s - m1 );
    }

    const QwtSplinePolynomial pnom = 
        QwtSplinePolynomial::fromSlopes( points[1], m1, points[2], m2 ); 

    const double cv2 = pnom.curvatureAt( 0.0 );

    double cv1;
    switch( boundaryCondition() )
    {
        case QwtSplineC1::Clamped2:
        {
            cv1 = boundaryValue;
            break;
        }
        case QwtSplineC1::Clamped3:
        {
            cv1 = cv2 - 6 * boundaryValue;
            break;
        }
        case QwtSplineC1::NotAKnot:
        {
            cv1 = cv2 - 6 * pnom.c3;
            break;
        }
        case QwtSplineC1::ParabolicRunout:
        {
            cv1 = cv2;
            break;
        }
        case QwtSplineC1::CubicRunout:
        {
            cv1 = 2 * cv2 - pnom.curvatureAt( 1.0 );
            break;
        }
        case QwtSplineC1::Natural:
        default:
            cv1 = 0.0;
    }


    const QwtSplinePolynomial pnomBegin =
        QwtSplinePolynomial::fromCurvatures( dx, dy, cv1, cv2 );

    return pnomBegin.slopeAt( 0.0 );
}

double QwtSplineC1::slopeEnd( const QPolygonF &points, double m1, double m2 ) const
{
    const int size = points.size();
    if ( size < 2 )
        return 0.0;

    const double boundaryValue = d_data->boundaryCondition.value[1];

    if ( boundaryCondition() == QwtSplineC1::Clamped )
        return boundaryValue;

    const double dx = points[size-1].x() - points[size-2].x();
    const double dy = points[size-1].y() - points[size-2].y();

    if ( boundaryCondition() == QwtSplineC1::LinearRunout )
    {
        const double s = dy / dx;
        return s - boundaryValue * ( s - m1 );
    }

    const QwtSplinePolynomial pnom = 
        QwtSplinePolynomial::fromSlopes( points[size-3], m1, points[size-2], m2 ); 

    const double cv1 = pnom.curvatureAt( points[size-2].x() - points[size-3].x() );

    double cv2;
    switch( boundaryCondition() )
    {
        case QwtSplineC1::Clamped2:
        {
            cv2 = boundaryValue;
            break;
        }
        case QwtSplineC1::NotAKnot:
        {
            cv2 = cv1 - 6 * pnom.c3;
            break;
        }
        case QwtSplineC1::Clamped3:
        {
            cv2 = cv1 - 6 * boundaryValue;
            break;
        }
        case QwtSplineC1::ParabolicRunout:
        {
            cv2 = cv1;
            break;
        }
        case QwtSplineC1::CubicRunout:
        {
            cv2 = 2 * cv1 - pnom.curvatureAt( 0.0 );
            break;
        }
        case QwtSplineC1::Natural:
        default:
            cv2 = 0.0;
    }

    const QwtSplinePolynomial pnomEnd = 
        QwtSplinePolynomial::fromCurvatures( dx, dy, cv1, cv2 );

    return pnomEnd.slopeAt( dx );
}

/*!
  \brief Calculate an interpolated painter path

  Interpolates a polygon piecewise into cubic Bezier curves
  and returns them as QPainterPath.

  The implementation calculates the slopes at the control points
  and converts them into painter path elements in an additional loop.
  
  \param points Control points
  \return QPainterPath Painter path, that can be rendered by QPainter

  \note Derived spline classes might overload painterPath() to avoid
        the extra loops for converting results into a QPainterPath

  \sa slopesParametric()
 */
QPainterPath QwtSplineC1::painterPath( const QPolygonF &points ) const
{
    const int n = points.size();
    if ( n <= 2 )
        return QwtSpline::painterPath( points );

    using namespace QwtSplineC1P;

    PathStore store;
    switch( parametrization()->type() )
    {
        case QwtSplineParametrization::ParameterX:
        {
            store = qwtSplineC1PathParamX<PathStore>( this, points );
            break;
        }
        case QwtSplineParametrization::ParameterY:
        {
            store = qwtSplineC1PathParamY<PathStore>( this, points );
            break;
        }
        case QwtSplineParametrization::ParameterUniform:
        {
            store = qwtSplineC1PathParametric<PathStore>(
                this, points, paramUniform() );
            break;
        }
        case QwtSplineParametrization::ParameterCentripetal:
        {
            store = qwtSplineC1PathParametric<PathStore>(
                this, points, paramCentripetal() );
            break;
        }
        case QwtSplineParametrization::ParameterChordal:
        {
            store = qwtSplineC1PathParametric<PathStore>(
                this, points, paramChordal() );
            break;
        }
        default:
        {
            store = qwtSplineC1PathParametric<PathStore>(
                this, points, param( parametrization() ) );
        }
    }

    return store.path;
}

/*! 
  \brief Interpolate a curve with Bezier curves
    
  Interpolates a polygon piecewise with cubic Bezier curves
  and returns the 2 control points of each curve as QLineF.

  \param points Control points
  \return Control points of the interpolating Bezier curves
 */
QVector<QLineF> QwtSplineC1::bezierControlLines( const QPolygonF &points ) const
{
    using namespace QwtSplineC1P;

    const int n = points.size();
    if ( n <= 2 )
        return QVector<QLineF>();

    ControlPointsStore store;
    switch( parametrization()->type() )
    {
        case QwtSplineParametrization::ParameterX:
        {
            store = qwtSplineC1PathParamX<ControlPointsStore>( this, points );
            break;
        }
        case QwtSplineParametrization::ParameterY:
        {
            store = qwtSplineC1PathParamY<ControlPointsStore>( this, points );
            break;
        }
        case QwtSplineParametrization::ParameterUniform:
        {
            store = qwtSplineC1PathParametric<ControlPointsStore>(
                this, points, paramUniform() );
            break;
        }
        case QwtSplineParametrization::ParameterCentripetal:
        {
            store = qwtSplineC1PathParametric<ControlPointsStore>(
                this, points, paramCentripetal() );
            break;
        }
        case QwtSplineParametrization::ParameterChordal:
        {
            store = qwtSplineC1PathParametric<ControlPointsStore>(
                this, points, paramChordal() );
            break;
        }
        default:
        {
            store = qwtSplineC1PathParametric<ControlPointsStore>(
                this, points, param( parametrization() ) );
        }
    }

    return store.controlPoints;
}

QPolygonF QwtSplineC1::equidistantPolygon( const QPolygonF &points,
    double distance, bool withNodes ) const
{
    if ( parametrization()->type() == QwtSplineParametrization::ParameterX )
    {
        if ( points.size() > 2 )
        {
            const QVector<double> m = slopes( points );
            if ( m.size() != points.size() )
                return QPolygonF();

            return qwtPolygonParametric<QwtSplinePolynomial::fromSlopes>(
                distance, points, m, withNodes );
        }
    }

    return QwtSplineG1::equidistantPolygon( points, distance, withNodes );
}

QVector<QwtSplinePolynomial> QwtSplineC1::polynomials(
    const QPolygonF &points ) const
{
    QVector<QwtSplinePolynomial> polynomials;

    const QVector<double> m = slopes( points );
    if ( m.size() < 2 )
        return polynomials;

    for ( int i = 1; i < m.size(); i++ )
    {
        polynomials += QwtSplinePolynomial::fromSlopes( 
            points[i-1], m[i-1], points[i], m[i] );
    }

    return polynomials;
}

QwtSplineC2::QwtSplineC2()
{
}

//! Destructor
QwtSplineC2::~QwtSplineC2()
{
}

/*!
  \brief Interpolate a curve with Bezier curves

  Interpolates a polygon piecewise with cubic Bezier curves
  and returns them as QPainterPath.

  \param points Control points
  \return Painter path, that can be rendered by QPainter
 */
QPainterPath QwtSplineC2::painterPath( const QPolygonF &points ) const
{
    // could be implemented from curvaturesX without the extra
    // loop for calculating the slopes vector. TODO ...

    return QwtSplineC1::painterPath( points );
}

/*! 
  \brief Interpolate a curve with Bezier curves
    
  Interpolates a polygon piecewise with cubic Bezier curves
  and returns the 2 control points of each curve as QLineF.

  \param points Control points
  \return Control points of the interpolating Bezier curves
 */
QVector<QLineF> QwtSplineC2::bezierControlLines( const QPolygonF &points ) const
{
    // could be implemented from curvaturesX without the extra
    // loop for calculating the slopes vector. TODO ...

    return QwtSplineC1::bezierControlLines( points );
}

QPolygonF QwtSplineC2::equidistantPolygon( const QPolygonF &points,
    double distance, bool withNodes ) const
{
    if ( parametrization()->type() == QwtSplineParametrization::ParameterX )
    {
        if ( points.size() > 2 )
        {
            const QVector<double> cv = curvatures( points );
            if ( cv.size() != points.size() )
                return QPolygonF();

            return qwtPolygonParametric<QwtSplinePolynomial::fromCurvatures>( 
                distance, points, cv, withNodes );
        }
    }

    return QwtSplineC1::equidistantPolygon( points, distance, withNodes );
}

QVector<double> QwtSplineC2::slopes( const QPolygonF &points ) const
{
    const QVector<double> curvatures = this->curvatures( points );
    if ( curvatures.size() < 2 )
        return QVector<double>();
    
    QVector<double> slopes( curvatures.size() );

    const double *cv = curvatures.constData();
    double *m = slopes.data();

    const int n = points.size();
    const QPointF *p = points.constData();

    QwtSplinePolynomial polynomial;

    for ( int i = 0; i < n - 1; i++ )
    {
        polynomial = QwtSplinePolynomial::fromCurvatures( p[i], cv[i], p[i+1], cv[i+1] );
        m[i] = polynomial.c1;
    }

    m[n-1] = polynomial.slopeAt( p[n-1].x() - p[n-2].x() );

    return slopes;
}

QVector<QwtSplinePolynomial> QwtSplineC2::polynomials( const QPolygonF &points ) const
{
    QVector<QwtSplinePolynomial> polynomials;
    
    const QVector<double> curvatures = this->curvatures( points );
    if ( curvatures.size() < 2 )
        return polynomials;

    const QPointF *p = points.constData();
    const double *cv = curvatures.constData();
    const int n = curvatures.size();
    
    for ( int i = 1; i < n; i++ )
    {   
        polynomials += QwtSplinePolynomial::fromCurvatures(
            p[i-1], cv[i-1], p[i], cv[i] );
    }
    
    return polynomials;
}
