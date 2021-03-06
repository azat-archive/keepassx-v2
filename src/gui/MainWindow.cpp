/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QtGui/QCloseEvent>
#include <QtGui/QShortcut>

#include "autotype/AutoType.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/FilePath.h"
#include "core/Metadata.h"
#include "gui/AboutDialog.h"
#include "gui/DatabaseWidget.h"
#include "gui/entry/EntryView.h"
#include "gui/group/GroupView.h"

const QString MainWindow::BaseWindowTitle = "KeePassX";

MainWindow::MainWindow()
    : m_ui(new Ui::MainWindow())
{
    m_ui->setupUi(this);

    setWindowIcon(filePath()->applicationIcon());
    QAction* toggleViewAction = m_ui->toolBar->toggleViewAction();
    toggleViewAction->setText(tr("Show toolbar"));
    m_ui->menuView->addAction(toggleViewAction);
    bool showToolbar = config()->get("ShowToolbar").toBool();
    m_ui->toolBar->setVisible(showToolbar);
    connect(m_ui->toolBar, SIGNAL(visibilityChanged(bool)), this, SLOT(saveToolbarState(bool)));

    m_clearHistoryAction = new QAction("Clear history", m_ui->menuFile);
    m_lastDatabasesActions = new QActionGroup(m_ui->menuRecentDatabases);
    connect(m_clearHistoryAction, SIGNAL(triggered()), this, SLOT(clearLastDatabases()));
    connect(m_lastDatabasesActions, SIGNAL(triggered(QAction*)), this, SLOT(openRecentDatabase(QAction*)));
    connect(m_ui->menuRecentDatabases, SIGNAL(aboutToShow()), this, SLOT(updateLastDatabasesMenu()));

    m_copyAdditionalAttributeActions = new QActionGroup(m_ui->menuEntryCopyAttribute);
    m_actionMultiplexer.connect(m_copyAdditionalAttributeActions, SIGNAL(triggered(QAction*)),
                                SLOT(copyAttribute(QAction*)));
    connect(m_ui->menuEntryCopyAttribute, SIGNAL(aboutToShow()),
            this, SLOT(updateCopyAttributesMenu()));

    Qt::Key globalAutoTypeKey = static_cast<Qt::Key>(config()->get("GlobalAutoTypeKey").toInt());
    Qt::KeyboardModifiers globalAutoTypeModifiers = static_cast<Qt::KeyboardModifiers>(
                config()->get("GlobalAutoTypeModifiers").toInt());
    if (globalAutoTypeKey > 0 && globalAutoTypeModifiers > 0) {
        autoType()->registerGlobalShortcut(globalAutoTypeKey, globalAutoTypeModifiers);
    }

    setShortcut(m_ui->actionDatabaseOpen, QKeySequence::Open, Qt::CTRL + Qt::Key_O);
    setShortcut(m_ui->actionDatabaseSave, QKeySequence::Save, Qt::CTRL + Qt::Key_S);
    setShortcut(m_ui->actionDatabaseSaveAs, QKeySequence::SaveAs);
    setShortcut(m_ui->actionDatabaseClose, QKeySequence::Close, Qt::CTRL + Qt::Key_W);
    m_ui->actionLockDatabases->setShortcut(Qt::CTRL + Qt::Key_L);
    setShortcut(m_ui->actionQuit, QKeySequence::Quit, Qt::CTRL + Qt::Key_Q);
    setShortcut(m_ui->actionSearch, QKeySequence::Find, Qt::CTRL + Qt::Key_F);
    m_ui->actionEntryNew->setShortcut(Qt::CTRL + Qt::Key_N);
    m_ui->actionEntryEdit->setShortcut(Qt::CTRL + Qt::Key_E);
    m_ui->actionEntryDelete->setShortcut(Qt::CTRL + Qt::Key_D);
    m_ui->actionEntryClone->setShortcut(Qt::CTRL + Qt::Key_K);
    m_ui->actionEntryCopyUsername->setShortcut(Qt::CTRL + Qt::Key_B);
    m_ui->actionEntryCopyPassword->setShortcut(Qt::CTRL + Qt::Key_C);
    setShortcut(m_ui->actionEntryAutoType, QKeySequence::Paste, Qt::CTRL + Qt::Key_V);
    m_ui->actionEntryOpenUrl->setShortcut(Qt::CTRL + Qt::Key_U);

#ifdef Q_OS_MAC
    new QShortcut(Qt::CTRL + Qt::Key_M, this, SLOT(showMinimized()));
#endif

    m_ui->actionDatabaseNew->setIcon(filePath()->icon("actions", "document-new"));
    m_ui->actionDatabaseOpen->setIcon(filePath()->icon("actions", "document-open"));
    m_ui->actionDatabaseSave->setIcon(filePath()->icon("actions", "document-save"));
    m_ui->actionDatabaseSaveAs->setIcon(filePath()->icon("actions", "document-save-as"));
    m_ui->actionDatabaseClose->setIcon(filePath()->icon("actions", "document-close"));
    m_ui->actionChangeDatabaseSettings->setIcon(filePath()->icon("actions", "document-edit"));
    m_ui->actionChangeMasterKey->setIcon(filePath()->icon("actions", "database-change-key", false));
    m_ui->actionLockDatabases->setIcon(filePath()->icon("actions", "document-encrypt", false));
    m_ui->actionQuit->setIcon(filePath()->icon("actions", "application-exit"));

    m_ui->actionEntryNew->setIcon(filePath()->icon("actions", "entry-new", false));
    m_ui->actionEntryClone->setIcon(filePath()->icon("actions", "entry-clone", false));
    m_ui->actionEntryEdit->setIcon(filePath()->icon("actions", "entry-edit", false));
    m_ui->actionEntryDelete->setIcon(filePath()->icon("actions", "entry-delete", false));

    m_ui->actionGroupNew->setIcon(filePath()->icon("actions", "group-new", false));
    m_ui->actionGroupEdit->setIcon(filePath()->icon("actions", "group-edit", false));
    m_ui->actionGroupDelete->setIcon(filePath()->icon("actions", "group-delete", false));

    m_ui->actionSettings->setIcon(filePath()->icon("actions", "configure"));

    m_ui->actionAbout->setIcon(filePath()->icon("actions", "help-about"));

    m_ui->actionSearch->setIcon(filePath()->icon("actions", "system-search"));

    m_actionMultiplexer.connect(SIGNAL(currentModeChanged(DatabaseWidget::Mode)),
                                this, SLOT(setMenuActionState(DatabaseWidget::Mode)));
    m_actionMultiplexer.connect(SIGNAL(groupChanged()),
                                this, SLOT(setMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(entrySelectionChanged()),
                                this, SLOT(setMenuActionState()));
    m_actionMultiplexer.connect(SIGNAL(groupContextMenuRequested(QPoint)),
                                this, SLOT(showGroupContextMenu(QPoint)));
    m_actionMultiplexer.connect(SIGNAL(entryContextMenuRequested(QPoint)),
                                this, SLOT(showEntryContextMenu(QPoint)));

    connect(m_ui->tabWidget, SIGNAL(tabNameChanged()),
            SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)),
            SLOT(updateWindowTitle()));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)),
            SLOT(databaseTabChanged(int)));
    connect(m_ui->tabWidget, SIGNAL(currentChanged(int)),
            SLOT(setMenuActionState()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(setMenuActionState()));
    connect(m_ui->stackedWidget, SIGNAL(currentChanged(int)), SLOT(updateWindowTitle()));
    connect(m_ui->settingsWidget, SIGNAL(editFinished(bool)), SLOT(switchToDatabases()));

    connect(m_ui->actionDatabaseNew, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(newDatabase()));
    connect(m_ui->actionDatabaseOpen, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(openDatabase()));
    connect(m_ui->actionDatabaseSave, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(saveDatabase()));
    connect(m_ui->actionDatabaseSaveAs, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(saveDatabaseAs()));
    connect(m_ui->actionDatabaseClose, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(closeDatabase()));
    connect(m_ui->actionChangeMasterKey, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(changeMasterKey()));
    connect(m_ui->actionChangeDatabaseSettings, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(changeDatabaseSettings()));
    connect(m_ui->actionImportKeePass1, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(importKeePass1Database()));
    connect(m_ui->actionLockDatabases, SIGNAL(triggered()), m_ui->tabWidget,
            SLOT(lockDatabases()));
    connect(m_ui->actionQuit, SIGNAL(triggered()), SLOT(close()));

    m_actionMultiplexer.connect(m_ui->actionEntryNew, SIGNAL(triggered()),
            SLOT(createEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryClone, SIGNAL(triggered()),
            SLOT(cloneEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryEdit, SIGNAL(triggered()),
            SLOT(switchToEntryEdit()));
    m_actionMultiplexer.connect(m_ui->actionEntryDelete, SIGNAL(triggered()),
            SLOT(deleteEntry()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyUsername, SIGNAL(triggered()),
            SLOT(copyUsername()));
    m_actionMultiplexer.connect(m_ui->actionEntryCopyPassword, SIGNAL(triggered()),
            SLOT(copyPassword()));
    m_actionMultiplexer.connect(m_ui->actionEntryAutoType, SIGNAL(triggered()),
            SLOT(performAutoType()));
    m_actionMultiplexer.connect(m_ui->actionEntryOpenUrl, SIGNAL(triggered()),
            SLOT(openUrl()));

    m_actionMultiplexer.connect(m_ui->actionGroupNew, SIGNAL(triggered()),
            SLOT(createGroup()));
    m_actionMultiplexer.connect(m_ui->actionGroupEdit, SIGNAL(triggered()),
            SLOT(switchToGroupEdit()));
    m_actionMultiplexer.connect(m_ui->actionGroupDelete, SIGNAL(triggered()),
            SLOT(deleteGroup()));

    connect(m_ui->actionSettings, SIGNAL(triggered()), SLOT(switchToSettings()));

    connect(m_ui->actionAbout, SIGNAL(triggered()), SLOT(showAboutDialog()));

    m_actionMultiplexer.connect(m_ui->actionSearch, SIGNAL(triggered()),
                                SLOT(toggleSearch()));
}

MainWindow::~MainWindow()
{
}

void MainWindow::updateLastDatabasesMenu()
{
    m_ui->menuRecentDatabases->clear();

    QStringList lastDatabases = config()->get("LastDatabases", QVariant()).toStringList();
    Q_FOREACH (const QString& database, lastDatabases) {
        QAction* action = m_ui->menuRecentDatabases->addAction(database);
        m_lastDatabasesActions->addAction(action);
    }
    m_ui->menuRecentDatabases->addSeparator();
    m_ui->menuRecentDatabases->addAction(m_clearHistoryAction);
}

void MainWindow::updateCopyAttributesMenu()
{
    DatabaseWidget* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
    if (!dbWidget) {
        return;
    }
    if (!dbWidget->entryView()->isSingleEntrySelected()) {
        return;
    }

    m_ui->menuEntryCopyAttribute->clear();
    Entry* entry = dbWidget->entryView()->currentEntry();

    Q_FOREACH (const QString& key, EntryAttributes::DefaultAttributes) {
        QAction* action = m_ui->menuEntryCopyAttribute->addAction(key);
        m_copyAdditionalAttributeActions->addAction(action);
    }

    m_ui->menuEntryCopyAttribute->addSeparator();

    Q_FOREACH (const QString& key, entry->attributes()->customKeys()) {
        QAction* action = m_ui->menuEntryCopyAttribute->addAction(key);
        m_copyAdditionalAttributeActions->addAction(action);
    }
}

void MainWindow::openRecentDatabase(QAction* action)
{
    openDatabase(action->text());
}

void MainWindow::clearLastDatabases()
{
    config()->set("LastDatabases", QVariant());
}

void MainWindow::openDatabase(const QString& fileName, const QString& pw, const QString& keyFile)
{
    m_ui->tabWidget->openDatabase(fileName, pw, keyFile);
}

void MainWindow::setMenuActionState(DatabaseWidget::Mode mode)
{
    bool inDatabaseTabWidget = (m_ui->stackedWidget->currentIndex() == 0);
    bool inWelcomeWidget = (m_ui->stackedWidget->currentIndex() == 2);

    if (inDatabaseTabWidget && m_ui->tabWidget->currentIndex() != -1) {
        DatabaseWidget* dbWidget = m_ui->tabWidget->currentDatabaseWidget();
        Q_ASSERT(dbWidget);

        if (mode == DatabaseWidget::None) {
            mode = dbWidget->currentMode();
        }

        switch (mode) {
        case DatabaseWidget::ViewMode: {
            bool inSearch = dbWidget->isInSearchMode();
            bool singleEntrySelected = dbWidget->entryView()->isSingleEntrySelected();
            bool groupSelected = dbWidget->groupView()->currentGroup();

            m_ui->actionEntryNew->setEnabled(!inSearch);
            m_ui->actionEntryClone->setEnabled(singleEntrySelected && !inSearch);
            m_ui->actionEntryEdit->setEnabled(singleEntrySelected);
            m_ui->actionEntryDelete->setEnabled(singleEntrySelected);
            m_ui->actionEntryCopyUsername->setEnabled(singleEntrySelected);
            m_ui->actionEntryCopyPassword->setEnabled(singleEntrySelected);
            m_ui->menuEntryCopyAttribute->setEnabled(singleEntrySelected);
            m_ui->actionEntryAutoType->setEnabled(singleEntrySelected);
            m_ui->actionEntryOpenUrl->setEnabled(singleEntrySelected);
            m_ui->actionGroupNew->setEnabled(groupSelected);
            m_ui->actionGroupEdit->setEnabled(groupSelected);
            m_ui->actionGroupDelete->setEnabled(groupSelected && dbWidget->canDeleteCurrentGoup());
            m_ui->actionSearch->setEnabled(true);
            // TODO: get checked state from db widget
            m_ui->actionSearch->setChecked(inSearch);
            m_ui->actionChangeMasterKey->setEnabled(true);
            m_ui->actionChangeDatabaseSettings->setEnabled(true);
            m_ui->actionDatabaseSave->setEnabled(true);
            m_ui->actionDatabaseSaveAs->setEnabled(true);
            break;
        }
        case DatabaseWidget::EditMode:
        case DatabaseWidget::LockedMode:
            Q_FOREACH (QAction* action, m_ui->menuEntries->actions()) {
                action->setEnabled(false);
            }

            Q_FOREACH (QAction* action, m_ui->menuGroups->actions()) {
                action->setEnabled(false);
            }
            m_ui->menuEntryCopyAttribute->setEnabled(false);

            m_ui->actionSearch->setEnabled(false);
            m_ui->actionSearch->setChecked(false);
            m_ui->actionChangeMasterKey->setEnabled(false);
            m_ui->actionChangeDatabaseSettings->setEnabled(false);
            m_ui->actionDatabaseSave->setEnabled(false);
            m_ui->actionDatabaseSaveAs->setEnabled(false);
            break;
        default:
            Q_ASSERT(false);
        }
        m_ui->actionDatabaseClose->setEnabled(true);
    }
    else {
        Q_FOREACH (QAction* action, m_ui->menuEntries->actions()) {
            action->setEnabled(false);
        }

        Q_FOREACH (QAction* action, m_ui->menuGroups->actions()) {
            action->setEnabled(false);
        }
        m_ui->menuEntryCopyAttribute->setEnabled(false);

        m_ui->actionSearch->setEnabled(false);
        m_ui->actionSearch->setChecked(false);
        m_ui->actionChangeMasterKey->setEnabled(false);
        m_ui->actionChangeDatabaseSettings->setEnabled(false);
        m_ui->actionDatabaseSave->setEnabled(false);
        m_ui->actionDatabaseSaveAs->setEnabled(false);

        m_ui->actionDatabaseClose->setEnabled(false);
    }

    bool inDatabaseTabWidgetOrWelcomeWidget = inDatabaseTabWidget || inWelcomeWidget;
    m_ui->actionDatabaseNew->setEnabled(inDatabaseTabWidgetOrWelcomeWidget);
    m_ui->actionDatabaseOpen->setEnabled(inDatabaseTabWidgetOrWelcomeWidget);
    m_ui->menuRecentDatabases->setEnabled(inDatabaseTabWidgetOrWelcomeWidget);
    m_ui->actionImportKeePass1->setEnabled(inDatabaseTabWidgetOrWelcomeWidget);

    m_ui->actionLockDatabases->setEnabled(m_ui->tabWidget->hasLockableDatabases());
}

void MainWindow::updateWindowTitle()
{
    QString customWindowTitlePart;
    int stackedWidgetIndex = m_ui->stackedWidget->currentIndex();
    int tabWidgetIndex = m_ui->tabWidget->currentIndex();
    if (stackedWidgetIndex == 0 && tabWidgetIndex != -1) {
        customWindowTitlePart = m_ui->tabWidget->tabText(tabWidgetIndex);
        if (m_ui->tabWidget->readOnly(tabWidgetIndex)) {
            customWindowTitlePart.append(QString(" [%1]").arg(tr("read-only")));
        }
    } else if (stackedWidgetIndex == 1) {
        customWindowTitlePart = tr("Settings");
    }

    QString windowTitle;
    if (customWindowTitlePart.isEmpty()) {
        windowTitle = BaseWindowTitle;
    } else {
        windowTitle = QString("%1 - %2").arg(customWindowTitlePart, BaseWindowTitle);
    }

    setWindowTitle(windowTitle);
}

void MainWindow::showAboutDialog()
{
    AboutDialog* aboutDialog = new AboutDialog(this);
    aboutDialog->show();
}

void MainWindow::switchToDatabases()
{
    if (m_ui->tabWidget->currentIndex() == -1) {
        m_ui->stackedWidget->setCurrentIndex(2);
    }
    else {
        m_ui->stackedWidget->setCurrentIndex(0);
    }
}

void MainWindow::switchToSettings()
{
    m_ui->settingsWidget->loadSettings();
    m_ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::databaseTabChanged(int tabIndex)
{
    if (tabIndex != -1 && m_ui->stackedWidget->currentIndex() == 2) {
        m_ui->stackedWidget->setCurrentIndex(0);
    }
    else if (tabIndex == -1 && m_ui->stackedWidget->currentIndex() == 0) {
        m_ui->stackedWidget->setCurrentIndex(2);
    }

    m_actionMultiplexer.setCurrentObject(m_ui->tabWidget->currentDatabaseWidget());
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_ui->tabWidget->closeAllDatabases()) {
        event->ignore();
    }
    else {
        event->accept();
    }
}

void MainWindow::showEntryContextMenu(const QPoint& globalPos)
{
    m_ui->menuEntries->popup(globalPos);
}

void MainWindow::showGroupContextMenu(const QPoint& globalPos)
{
    m_ui->menuGroups->popup(globalPos);
}

void MainWindow::saveToolbarState(bool value)
{
    config()->set("ShowToolbar", value);
}

void MainWindow::setShortcut(QAction* action, QKeySequence::StandardKey standard, int fallback)
{
    if (!QKeySequence::keyBindings(standard).isEmpty()) {
        action->setShortcuts(standard);
    }
    else if (fallback != 0) {
        action->setShortcut(QKeySequence(fallback));
    }
}
