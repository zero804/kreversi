#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QTimer>

#include <kdebug.h>
#include <kstandarddirs.h>

#include "kreversiscene.h"
#include "kreversigame.h"
#include "kreversichip.h"


// FIXME dimsuz: something to remove/give-more-thinking
const int CHIP_SIZE = 36;

KReversiScene::KReversiScene( KReversiGame* game , const QPixmap& chipsPixmap )
{
    setBackgroundBrush( Qt::lightGray );

    m_boardRect = QRectF(5, 5, CHIP_SIZE*8, CHIP_SIZE*8);
    setSceneRect( 0, 0, m_boardRect.width()+10, m_boardRect.height()+10);

    m_frameSet = new KReversiChipFrameSet( chipsPixmap, CHIP_SIZE );
    m_animTimer = new QTimer(this);
    connect(m_animTimer, SIGNAL(timeout()), SLOT(slotAnimationStep()));

    setGame(game);
}

void KReversiScene::setGame( KReversiGame* game )
{
    m_game = game;
    // FIXME dimsuz: is it needed?
    connect( m_game, SIGNAL(boardChanged()), this, SLOT(updateBoard()) );
    connect( m_game, SIGNAL(moveFinished()), this, SLOT(slotMoveFinished()) );

    // this will remove all items left from previous game
    QList<QGraphicsItem*> allChips = items( m_boardRect );
    foreach( QGraphicsItem* chip, allChips )
    {
        removeItem( chip );
        delete chip;
    }

    updateBoard();
}

void KReversiScene::updateBoard()
{
    for(int row=0; row<8; ++row)
        for(int col=0; col<8; ++col )
        {
            ChipColor color = m_game->chipColorAt( row, col );
            if( color != NoColor )
            {
                // if there's a chip, just change it color
                // otherwise create new
                KReversiChip *chip = static_cast<KReversiChip*>(itemAt( cellCenter(row, col) ));
                if( chip != 0 )
                {
                    if( chip->color() != color )
                    {
                        kDebug() << "Found item at (" << row << "," << col << "). Setting its color." << endl;
                        chip->setColor( color );
                    }
                }
                else
                {
                    kDebug() << "No item at (" << row << "," << col << "). Creating." << endl;
                    chip = new KReversiChip( color, m_frameSet, this );
                    chip->setPos( cellTopLeft(row, col) );
                }
            }
            else
            {
                // this if-branch happens on undos
                delete itemAt( cellCenter(row, col) );
            }
        }
}

void KReversiScene::slotMoveFinished()
{
    m_changedChips = m_game->changedChips();
    // create an item that was placed by player
    // by convention it will be the first in the list of changed items
    KReversiMove move = m_changedChips.takeFirst();
    KReversiChip *newchip = new KReversiChip( move.color, m_frameSet, this );
    newchip->setPos( cellTopLeft( move.row, move.col ) );
    // start animation
    m_animTimer->start(20);
}

void KReversiScene::slotAnimationStep()
{
    KReversiMove move = m_changedChips.at(0);
    KReversiChip *chip = static_cast<KReversiChip*>(itemAt( cellCenter(move.row, move.col) ));

    bool animFinished = chip->nextFrame();
    if(animFinished)
    {
        chip->setColor( move.color );

        m_changedChips.removeFirst(); // we finished animating it

        if(m_changedChips.count() == 0)
        {
            kDebug() << "stopping timer" <<endl;
            m_animTimer->stop();
            // next turn
            if( m_game->computersTurn() )
            {
                m_game->makeComputerMove();
            }
        }
    }
}

QPointF KReversiScene::cellCenter( int row, int col ) const
{
    return QPointF( m_boardRect.x() + col*CHIP_SIZE + CHIP_SIZE/2, m_boardRect.y() + row*CHIP_SIZE + CHIP_SIZE/2 );
}

QPointF KReversiScene::cellTopLeft( int row, int col ) const
{
    return QPointF( m_boardRect.x() + col*CHIP_SIZE, m_boardRect.y() + row*CHIP_SIZE );
}

void KReversiScene::setBackgroundPixmap( const QPixmap& pix )
{
    m_bkgndPix = pix;
    update();
}

void KReversiScene::drawBackground( QPainter *p, const QRectF& r)
{
    if(!m_bkgndPix.isNull())
    {
        p->setBrush( m_bkgndPix );
        p->drawRect( r );
    }

    QPen pen(Qt::black);
    pen.setWidth(2);

    p->setPen(pen);

    qreal startx = m_boardRect.x();
    qreal starty = m_boardRect.y();
    qreal endx = m_boardRect.x() + m_boardRect.width();
    qreal endy = m_boardRect.y() + m_boardRect.height();
    
    for(qreal x=m_boardRect.x(); x<=endx; x+=CHIP_SIZE)
        p->drawLine( QPointF(x, starty), QPointF(x, endy) );
    for(qreal y=m_boardRect.y(); y<=endy; y+=CHIP_SIZE)
        p->drawLine( QPointF(startx, y), QPointF(endx, y) );
}

void KReversiScene::mousePressEvent( QGraphicsSceneMouseEvent* ev )
{
    if( m_game->computersTurn() )
    {
        kDebug() << "Not your turn, player" << endl;
        return;
    }

    if( !m_boardRect.contains(ev->scenePos()) )
        return;
    int row = (int)ev->scenePos().y() / CHIP_SIZE;
    int col = (int)ev->scenePos().x() / CHIP_SIZE;

    if( row < 0 ) row = 0;
    if( row > 7 ) row = 7;
    if( col < 0 ) col = 0;
    if( col > 7 ) col = 7;
    
    kDebug() << "Cell (" << row << "," << col << ") clicked." << endl;

    m_game->makePlayerMove( row, col );
}

#include "kreversiscene.moc"
