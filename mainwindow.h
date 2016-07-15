#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//#define TEST

#include <QMainWindow>
#include <QTimer>
#include <QTime>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QFile>
#include <QDebug>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLCDNumber>
#include <QPalette>
#include <QMessageBox>
#include <QDialog>
#include <QCloseEvent>
#include <QBrush>
#include <QHash>
#include <QFileDialog>
#include <QDir>
#include <QUrl>
#include <QDesktopServices>
#include <QSettings>
#include <QTextCodec>
#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraViewfinder>
#include <QtCore/qmath.h>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPointer>

#ifdef TEST
    #include <qglobal.h>
#endif

//for printing
#include <QtPrintSupport/QPrinter>
#include <QPainter>
#include <QProgressDialog>
#include <QApplication>
#include <QTextDocument>
#include <QTextTableCell>
#include <QTextCursor>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

signals:
    void autoSave(QString fileName);
    void debug();

private slots:
    void oneSecondeActions();
    void loadListing();
    void stopClicked();
    void readConfig();
    void writeConfig();
    void startClicked();
    void restoreStartedRace();
    void modifyResults(int rowSel, int colSel);
    void enterNumberRacer();
    void saveFinishersResults();
    void printFinishersResults();
    void saveResults(QString fileName);
    void setCamera(int numDevice);
    void enableCamera(int state);
    void displayCameraError();
    void enableAutosave(int state);
    void setSyncTimeAutoSave(int value);
    void delNumberRacer();
    void takePicture();
    void addCheckPoint();
    void deleteCheckPoint();
    void enableLaps(bool state);

#ifdef TEST
    void runTestStep();
#endif

private:
    QString raceName;

    //option
    bool autoSaveEnable;
    bool useCamera;
    int syncTimeAutoSave;

    //for time of race
    QLCDNumber* showExternTime;
    QLabel * showRacersResults;
    QTimer *timer; //timer to get tick every second
    QDateTime initStart; //start time
    int elapsedTime; //ticks count for the beginning of race
    int deltaTime; //time since start time to current time to manage saved race
    int syncTime; //delta time to do action
    void countTime();
    void showTime();
    QString getTime(const char c_format);

    //used for listing and results
    //labels for ini files and headers listing
    QString str_bib;
    QString str_name;
    QString str_frstname;
    QString str_order;
    QString str_subRace;
    QString str_assoc;
    QString str_genre;
    QString str_scratch;
    QString str_time;
    QString str_loop;

    //to manage racers' status
    int countListingRacers;//!amount of whole racers who are in the listing
    QList<int> countWholeRacersList; //!amount of racers who are in the listing for each subrace
    QList<int> countRacersList; //!amount of racers added step by step for each subrace
    QList<int> countFinishersList; //!amount of racers who reach the last check point for each subrace
    QList<QProgressBar*> subRacesProgress;

    //to manage race, subraces and their characteristics
    QList<QTableWidget*> subRacesResults;
    QStringList subRaces;
    QHash<QString, int> numberOfChkPts; //!number of checkPoints for each subrace
    bool bLoopRace;

    //attr for the listing stuff
    QStringList labelsListing; //! labels we want to search in the imported listing file and show int the ui
    QHash<QString, int> hdrListing; //!index of the labels header in listing

    //attr for the results stuff
    QList<QStringList> hdrResults; //!index of the labels header (with checkpoints) in results list for each subrace
    QList<QStringList> labelChkPts; //! labels of the checkpoints for each subrace
    QStringList labelChkPtsList; //!list of the possible chekpoints from ini file

    //functions
    void classRacers(void);
    bool importFromCSV(QString nameFile, QTableWidget* table, bool withRowHeader);
    bool importListingFromCSV(QString nameFile, QTableWidget *table);
    bool importFromResultsCSV(QString nameFile, bool withRowHeader);
    QStringList processLineFromCSV(QString line);
    int rowInListing(QString numRacer);
    int inWhichSubRace(QString numRacer); //return the index of the subrace
    int addScore(int idxSubRace,QString racerNumber,QString s_time, int numChkPts, int selRow);
    bool initRaceCharacteristics(QTableWidget* table);
    void updateLive(int totalRanked);
    int countRacers(QTableWidget *table, QString str_subRaceChosen);
    void newCheckPoint(int idxSubRace, QString checkPointName); //surcharge impossible car déjà en signal

    //used for printing
    static const int textMargins = 12; // in millimeters
    static const int borderMargins = 10; // in millimeters
    void printResultsHTML(QString fileName);
    void printResultsPDF(QString fileName);
    void addTable(QTextCursor &cursor, int idxSubRace);
    void printDocument(QPrinter &printer, QTextDocument *doc, QWidget *parentWidget);
    void paintPage(QPrinter &printer, int pageNumber, int pageCount, QPainter *painter, QTextDocument *doc, const QRectF &textRect, qreal footerHeight);
    double mmToPixels(QPrinter &printer, int mm);

    //Main gui purpose
    QPalette normalPalette;
    QPalette hilightPalette;
    Ui::MainWindow *ui;
    void closeEvent(QCloseEvent *event);

    //for camera
    QPointer<QCamera> camera;
    QPointer<QCameraImageCapture> imageCapture;
    QCameraViewfinder *viewfinder;
    bool cameraEnabled;

#ifdef TEST
    //for u tests
    QVector<QString> testBids;
    bool testDone;
    QTimer *testTimer;
#endif
    int addLap(int idxSubRace, int selRow);
    void showRacersInformation(void);
};

/*!
 * \brief The Dialog class to choose the subraces when we load a listing
 */
class Dialog : public QDialog
{
    Q_OBJECT
public:
    Dialog(QStringList* a_oResult);
private slots:
    void changeList(void);
private:
    QStringList *m_oResult;
    QList<QCheckBox*> chkBoxList;
};

#endif // MAINWINDOW_H
