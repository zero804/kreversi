/* Yo Emacs, this is -*- C++ -*-
 *******************************************************************
 *******************************************************************
 *
 *
 * KREVERSI
 *
 *
 *******************************************************************
 *
 * A Reversi (or sometimes called Othello) game
 *
 *******************************************************************
 *
 * created 1997 by Mario Weilguni <mweilguni@sime.com>
 *
 *******************************************************************
 *
 * This file is part of the KDE project "KREVERSI"
 *
 * KREVERSI is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * KREVERSI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with KREVERSI; see the file COPYING.  If not, write to
 * the Free Software Foundation, 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 *******************************************************************
 */


#include <unistd.h>

#include <qlayout.h>

#include <kapplication.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kaction.h>
#include <kstdgameaction.h>
#include <kkeydialog.h>
#include <kconfigdialog.h>
#include <knotifyclient.h>
#include <knotifydialog.h>
#include <kexthighscore.h>

#include "prefs.h"
#include "Score.h"
#include "kreversi.h"
#include "kreversi.moc"
#include "board.h"
#include "settings.h"


// ================================================================
//                     class StatusWidget


StatusWidget::StatusWidget(const QString &text, QWidget *parent)
  : QWidget(parent, "status_widget")
{
  QHBoxLayout  *hbox  = new QHBoxLayout(this, 0, KDialog::spacingHint());
  QLabel       *label;

  label = new QLabel(text, this);
  hbox->addWidget(label);

  m_pixLabel = new QLabel(this);
  hbox->addWidget(m_pixLabel);

  label = new QLabel(":", this);
  hbox->addWidget(label);

  m_label = new QLabel(this);
  hbox->addWidget(m_label);
}


// Set the text label - used to write the number of pieces.
//

void StatusWidget::setScore(uint s)
{
  m_label->setText(QString::number(s));
}


// Set the pixel label - used to show the color.
//

void StatusWidget::setPixmap(const QPixmap &pixmap)
{
  m_pixLabel->setPixmap(pixmap);
}


// ================================================================
//                     class KReversi


#ifndef PICDATA
#define PICDATA(x)  \
    KGlobal::dirs()->findResource("appdata", QString("pics/") + x)
#endif


KReversi::KReversi()
  : KZoomMainWindow(10, 300, 5, "kreversi"), 
    gameOver(false)
{
  QWidget     *w;
  QHBoxLayout *top;

  KNotifyClient::startDaemon();

  // The game.
  m_game       = new Game();
  cheating     = false;
  m_humanColor = Black;

  w = new QWidget(this);
  setCentralWidget(w);

  top = new QHBoxLayout(w);
  top->addStretch(1);

  // The reversi board view.
  m_board = new Board(w, m_game);
  top->addWidget(m_board);
  top->addStretch(1);

  // The Engine
  m_engine = new Engine();
  setStrength(1);
  
  // Populate the GUI.
  createStatusBar();
  createKActions();
  addWidget(m_board);

  // Connect some signals on the board with slots of the application
  // FIXME: Look through all signals and see if they make sense,
  //        both from this class and from Board.
  connect(m_board, SIGNAL(score()),        this, SLOT(slotScore()));
  connect(this,    SIGNAL(score()),        this, SLOT(slotScore()));
  connect(m_board, SIGNAL(gameWon(Color)), this, SLOT(slotGameEnded(Color)));
  connect(m_board, SIGNAL(turn(Color)),    this, SLOT(slotTurn(Color)));
  connect(this,    SIGNAL(turn(Color)),    this, SLOT(slotTurn(Color)));
  connect(this,    SIGNAL(stateChange(State)),
          this,    SLOT(slotStateChange(State)));

  connect(m_board, SIGNAL(signalSquareClicked(int, int)),
	  this,    SLOT(slotSquareClicked(int, int)));


  loadSettings();

  setupGUI();
  init("popup");
  m_board->start();
}

KReversi::~KReversi()
{
  delete m_game;
  delete m_engine;
}



// Create the status bar at the lower edge of the main window.
//

void KReversi::createStatusBar()
{
  m_humanStatus = new StatusWidget(i18n("You"), this);
  statusBar()->addWidget(m_humanStatus, 0, true);

  m_computerStatus = new StatusWidget(QString::null, this);
  statusBar()->addWidget(m_computerStatus, 0, true);
}


// Create all KActions used in KReversi.
//

void KReversi::createKActions()
{
  // Standard Game Actions.
  KStdGameAction::gameNew(this, SLOT(slotNewGame()),  actionCollection(), 
			  "game_new");
  KStdGameAction::load(this,    SLOT(slotOpenGame()), actionCollection());
  KStdGameAction::save(this,    SLOT(slotSave()),     actionCollection());
  KStdGameAction::quit(this,    SLOT(close()),        actionCollection());
  KStdGameAction::hint(this,    SLOT(slotHint()),     actionCollection(),
		       "game_hint");
  KStdGameAction::undo(this,    SLOT(slotUndo()),     actionCollection(),
		       "game_undo");

  // Non-standard Game Actions: Stop, Continue, Switch sides
  stopAction = new KAction(i18n("&Stop Thinking"), "game_stop", Qt::Key_Escape,
			   this, SLOT(slotInterrupt()), actionCollection(),
			   "game_stop");
  continueAction = new KAction(i18n("&Continue Thinking"), "reload", 0,
			       this, SLOT(slotContinue()), actionCollection(),
			       "game_continue");
  new KAction(i18n("S&witch Sides"), 0, 0,
	      this, SLOT(slotSwitchSides()), actionCollection(),
	      "game_switch_sides");

  // Some more standard game actions: Highscores, Settings.
  KStdGameAction::highscores(this, SLOT(showHighScoreDialog()), actionCollection());
  KStdAction::preferences(this, SLOT(slotEditSettings()), actionCollection());
}


// ----------------------------------------------------------------
//                    Methods for the engine


void KReversi::setStrength(uint strength)
{
  // FIXME: 7 should be MAXSTRENGTH or something similar.
  Q_ASSERT( 1 <= strength && strength <= 7 );

  strength = QMAX(QMIN(strength, 7), 1);
  m_engine->setStrength(strength);
  if (m_lowestStrength < strength)
    m_lowestStrength = strength;
  KExtHighscore::setGameType(m_lowestStrength-1);
}


// ----------------------------------------------------------------
//                        Slots for KActions


// A slot that is called when the user wants a new game.
//

void KReversi::slotNewGame()
{
  // If we are already playing, treat this as a loss.
  if ( isPlaying() )
    KExtHighscore::submitScore(KExtHighscore::Lost, this);

  gameOver = false;
  cheating = false;

  m_game->reset();
  m_competitiveGame = Prefs::competitiveGameChoice();
  m_lowestStrength  = strength();
  //kdDebug() << "Competitive: " << m_competitiveGame << endl;


  // Show some data
  setState(Ready);
  emit turn(Black);
  // FIXME: emit a signal instead (signalBoard(bool)?).
  m_board->updateBoard(TRUE);

  // Black always makes first move.
  if (m_humanColor == White)
    computerMakeMove();
}


// Open an earlier saved game from the config file.
//
// FIXME: Should give a choice to load from an ordinary file (SGF?)
//

void KReversi::slotOpenGame()
{
  KConfig *config = kapp->config();
  config->setGroup("Savegame");

  if (loadGame(config))
    Prefs::setSkill(m_engine->strength());
  updateColors();
}


// Save a game to the config file.
//
// FIXME: Should give a choice to save as an ordinary file (SGF?)
//

void KReversi::slotSave()
{
  KConfig *config = kapp->config();
  config->setGroup("Savegame");
  saveGame(config);

  KMessageBox::information(this, i18n("Game saved."));
}


void KReversi::slotHint()
{
  Move  move;

  if (state() != Ready) 
    return;

  setState(Thinking);
  move = m_engine->computeMove(*m_game, m_competitiveGame);

  setState(Hint);
  m_board->showHint(move);

  setState(Ready);
}


// Takes back last set of moves
//

void KReversi::slotUndo()
{
  if (state() != Ready)
    return;

  // Can't undo anything if no moves are made.
  if (m_game->moveNumber() == 0)
    return;

  // Undo all moves of the same color as the last one.
  Color  last_color = m_game->lastMove().color();
  while (m_game->moveNumber() != 0
	 && last_color == m_game->lastMove().color())
    m_game->takeBackMove();

  // Take back one more move.
  m_game->takeBackMove();

  if (m_game->toMove() == computerColor()) {
    // Must repaint so that the new move is not shown before the old
    // one is removed on the screen.
    m_board->repaint();
    computerMakeMove();
  }
  else
    m_board->update();
}


// Interrupt thinking of game engine.
//

void KReversi::slotInterrupt()
{
  m_engine->setInterrupt(TRUE);
}


// Continues a move if it was interrupted earlier.
//

void KReversi::slotContinue()
{
  if (interrupted())
    computerMakeMove();
}


void KReversi::slotSwitchSides()
{
  if (state() != Ready) 
    return;

  // It's ok to change sides before the first move.
  if (m_game->moveNumber() != 0) {
    int res = KMessageBox::warningContinueCancel(this,
						 i18n("If you switch side, your score will not be added to the highscores."),
						 QString::null, QString::null, "switch_side_warning");
    if ( res==KMessageBox::Cancel ) 
      return;

    cheating = true;
  }

  m_humanColor = opponent(m_humanColor);

  emit score();
  updateColors();
  kapp->processEvents();
  computerMakeMove();
}


// ----------------------------------------------------------------
//                   Slots for the game IO


// Handle mouse clicks.
//

void KReversi::slotSquareClicked(int row, int col)
{
  // Can't move when it is the computers turn.
  if ( interrupted() ) {
    illegalMove();
    return;
  }

  if (state() == Ready)
    humanMakeMove(row, col);
  else if (state() == Hint) {
    m_board->quitHint();
    setState(Ready);
  }
  else
    illegalMove();
}


// ----------------------------------------------------------------
//                   Slots for the game view


// Show the number of pieces for each side.
//

void KReversi::slotScore()
{
  m_humanStatus   ->setScore(m_game->score(humanColor()));
  m_computerStatus->setScore(m_game->score(computerColor()));
}


// A slot that is called when the game ends.
//

void KReversi::slotGameEnded(Color color) 
{
  // If the game already was over, do nothing.
  if (gameOver)
    return;

  statusBar()->message(i18n("End of game"));

  // Get the scores.
  uint human    = m_game->score(humanColor());
  uint computer = m_game->score(computerColor());

  KExtHighscore::Score score;
  score.setScore(m_game->score(humanColor()));
  
  // Show the winner in a messagebox.
  if ( color == Nobody ) {
    KNotifyClient::event(winId(), "draw", i18n("Draw!"));
    QString s = i18n("Game is drawn!\n\nYou     : %1\nComputer: %2")
                .arg(human).arg(computer);
    KMessageBox::information(this, s, i18n("Game Ended"));
    score.setType(KExtHighscore::Draw);
  }
  else if ( humanColor() == color ) {
    KNotifyClient::event(winId(), "won", i18n("Game won!"));
    QString s = i18n("Congratulations, you have won!\n\nYou     : %1\nComputer: %2")
                .arg(human).arg(computer);
    KMessageBox::information(this, s, i18n("Game Ended"));
    score.setType(KExtHighscore::Won);
  } 
  else {
    KNotifyClient::event(winId(), "lost", i18n("Game lost!"));
    QString s = i18n("You have lost the game!\n\nYou     : %1\nComputer: %2")
                .arg(human).arg(computer);
    KMessageBox::information(this, s, i18n("Game Ended"));
    score.setType(KExtHighscore::Lost);
  }
  
  // Store the result in the highscore file if no cheating was done,
  // and only if the game was competitive.
  if ( !cheating && m_competitiveGame) {
    KExtHighscore::submitScore(score, this);
  }
  gameOver = true;
}


// A slot that is called when it is time to show whose turn it is.
//

void KReversi::slotTurn(Color color)
{
  // If we are not playing, do nothing. 
  if (gameOver)
    return;

  if (color == humanColor())
    statusBar()->message(i18n("Your turn"));
  else if (color == computerColor())
    statusBar()->message(i18n("Computer's turn"));
  else
    statusBar()->clear();
}


// A slot that is called when the state of the program changes.
//

void KReversi::slotStateChange(State state)
{
  if (state == Thinking){
    kapp->setOverrideCursor(waitCursor);
    stopAction->setEnabled(true);
  }
  else {
    kapp->restoreOverrideCursor();
    stopAction->setEnabled(false);
  }

  continueAction->setEnabled(interrupted());
}


// ----------------------------------------------------------------
//                        Private methods


// Handle the humans move.
//

void KReversi::humanMakeMove(int row, int col)
{
  if (state() != Ready) 
    return;

  Color color = m_game->toMove();

  // Create a move from the mouse click and see if it is legal.
  // If it is, then make a human move.
  Move  move(color, col + 1, row + 1);
  if (m_game->moveIsLegal(move)) {
    m_game->makeMove(move);
    m_board->animateChanged(move);

    if (!m_game->moveIsAtAllPossible()) {
      m_board->updateBoard();
      setState(Ready);
      m_board->gameEnded();
      return;
    }

    m_board->updateBoard();

    if (color != m_game->toMove())
      computerMakeMove();
  } else
    illegalMove();
}


// Make a computer move.
//

void KReversi::computerMakeMove()
{
  // Check if the computer can move.
  Color color    = m_game->toMove();
  Color opponent = ::opponent(color);

  emit turn(color);

  if (!m_game->moveIsPossible(color))
    return;
 
  // Make computer moves until the human can play or until the game is over.
  setState(Thinking);
  do {
    Move  move;

    if (!m_game->moveIsAtAllPossible()) {
      setState(Ready);
      m_board->gameEnded();
      return;
    }

    move = m_engine->computeMove(*m_game, m_competitiveGame);
    if (move.x() == -1) {
      setState(Ready);
      return;
    }
    usleep(300000); // Pretend we have to think hard.

    //playSound("click.wav");
    m_game->makeMove(move);
    m_board->animateChanged(move);
    m_board->updateBoard();
  } while (!m_game->moveIsPossible(opponent));


  emit turn(opponent);
  setState(Ready);

  if (!m_game->moveIsAtAllPossible()) {
    m_board->gameEnded();
    return;
  }
}


// Handle an attempt to make an illegal move by the human.

void KReversi::illegalMove()
{
  KNotifyClient::event(winId(), "illegal_move", i18n("Illegal move"));
}


// Saves the game in the config file.  
//
// Only one game at a time can be saved.
//

void KReversi::saveGame(KConfig *config)
{
  // Stop thinking.
  slotInterrupt(); 

  // Write the data to the config file.
  config->writeEntry("NumberOfMoves", moveNumber());
  config->writeEntry("State", state());
  config->writeEntry("Strength", strength());

  // Write the moves of the game to the config object.  This object
  // saves itself all at once so we don't have to write the moves
  // to the file ourselves.
  for (uint i = moveNumber(); i > 0; i--) {
    Move  move = m_game->lastMove();
    m_game->takeBackMove();

    QString s, idx;
    s.sprintf("%d %d %d", move.x(), move.y(), (int)move.color());
    idx.sprintf("Move_%d", i);
    config->writeEntry(idx, s);
  }

  // Save whose turn it is and if the game is competitive.
  config->writeEntry("Competitive", (int) m_competitiveGame);
  config->writeEntry("HumanColor",  (int) m_humanColor);
  config->sync();

  // All moves must be redone.
  loadGame(config, TRUE);

  // Continue with the move if applicable.
  slotContinue(); 
}


// Loads the game.  Only one game at a time can be saved.
//

bool KReversi::loadGame(KConfig *config, bool noupdate)
{
  slotInterrupt(); // stop thinking

  uint  nmoves = config->readNumEntry("NumberOfMoves", 0);
  if (nmoves==0) 
    return false;

  m_game->reset();
  uint movenumber = 1;
  while (nmoves--) {
    // Read one move.
    QString  idx;
    idx.sprintf("Move_%d", movenumber++);

    QStringList  s = config->readListEntry(idx, ' ');
    uint         x = (*s.at(0)).toUInt();
    uint         y = (*s.at(1)).toUInt();
    Color        color = (Color)(*s.at(2)).toInt();

    Move  move(color, x, y);
    m_game->makeMove(move);
  }

  m_humanColor      = (Color) config->readNumEntry("HumanColor");
  m_competitiveGame = (bool)  config->readNumEntry("Competitive");

  if (noupdate)
    return true;

  m_board->updateBoard(TRUE);
  setState(State(config->readNumEntry("State")));
  setStrength(config->readNumEntry("Strength", 1));
  //kdDebug() << "Competitive set to: " << m_competitiveGame << endl;

  if (interrupted())
    slotContinue();
  else {
    // Make the view show who is to move.
    emit turn(m_game->toMove());

    // Computer makes first move.
    if (m_humanColor != m_game->toMove())
      computerMakeMove();
  }

  return true;
}


// ----------------------------------------------------------------


void KReversi::saveProperties(KConfig *c)
{
  saveGame(c);
}


void KReversi::readProperties(KConfig *c) {
  loadGame(c);
  gameOver = false;
  cheating = false;		// FIXME: Is this true?
}


void KReversi::showHighScoreDialog()
{
  KExtHighscore::show(this);
}


void KReversi::slotEditSettings()
{
  // If we are already editing the settings, then do nothing.
  if (KConfigDialog::showDialog("settings"))
    return;

  KConfigDialog *dialog  = new KConfigDialog(this, "settings", Prefs::self(), 
					     KDialogBase::Swallow);
  Settings      *general = new Settings(0, "General");

  dialog->addPage(general, i18n("General"), "package_settings");
  connect(dialog, SIGNAL(settingsChanged()), this, SLOT(loadSettings()));
  dialog->show();
}


void KReversi::configureNotifications()
{
    KNotifyDialog::configure(this);
}


void KReversi::updateColors()
{
  m_humanStatus   ->setPixmap(m_board->chipPixmap(humanColor(), 20));
  m_computerStatus->setPixmap(m_board->chipPixmap(computerColor(), 20));
}


void KReversi::loadSettings()
{
  m_humanColor = (Color) Prefs::humanColor();
  setStrength(Prefs::skill());

  // m_competitiveGame is set at the start of a game and can only be
  // downgraded during the game, never upgraded.
  if ( !Prefs::competitiveGameChoice() )
    m_competitiveGame = false;

  m_board->loadSettings();

  // Show the color of the human and the computer.
  updateColors(); 
}


bool KReversi::isPlaying() const
{
  return ( m_game->moveNumber() != 0 && !gameOver );
}


void KReversi::setState(State newState)
{
  m_state = newState;
  emit stateChange(m_state);
}


bool KReversi::queryExit()
{
  if ( isPlaying() )
    KExtHighscore::submitScore(KExtHighscore::Lost, this);

  return KZoomMainWindow::queryExit();
}


void KReversi::writeZoomSetting(uint zoom)
{
  Prefs::setZoom(zoom);
  Prefs::writeConfig();
}


uint KReversi::readZoomSetting() const
{
  return Prefs::zoom();
}


void KReversi::writeMenubarVisibleSetting(bool visible)
{
  Prefs::setMenubarVisible(visible);
  Prefs::writeConfig();
}


bool KReversi::menubarVisibleSetting() const
{
  return Prefs::menubarVisible();
}
