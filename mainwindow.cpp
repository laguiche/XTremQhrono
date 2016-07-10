#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //indice pour les boucles conditionnelles
    int i=0;

    //recupere les infos du fichier de config:
    //labels des listings, nombre et noms des courses,....
    readConfig();

    //init des données
    countListingRacers=0;

    //affichage du temps pour les coureurs
    showExternTime=new QLCDNumber();
    showExternTime->setDigitCount(8);
    showExternTime->setSegmentStyle(QLCDNumber::Flat);
    QPalette Pal(palette());
    Pal.setColor(QPalette::Background, Qt::black); //fond noir
    Pal.setColor(QPalette::Foreground, Qt::red); //chiffres rouges
    showExternTime->setAutoFillBackground(true);
    showExternTime->setPalette(Pal);
    showExternTime->setGeometry(300,300,140,30);
    showExternTime->setMinimumSize(116,23);
    showExternTime->show();

    //Init du timer principal qui va cadencer les actions a 1 seconde
    //et du chrono global
    timer = new QTimer(this);
    elapsedTime=0;
    deltaTime=0;
    ui->raceTimerLCD->display("00:00:00");
    connect(timer, SIGNAL(timeout()), this, SLOT(oneSecondeActions()));
    connect(ui->startButton,SIGNAL(clicked()),this,SLOT(startClicked()));
    connect(ui->stopButton,SIGNAL(clicked()),this,SLOT(stopClicked()));

    //Init du widget gerant le listing coureur
    labelsListing=(QStringList()<<str_bib <<str_name<<str_frstname<<str_genre<<str_assoc<<str_subRace);
    ui->tableWidget_Listing->setColumnCount(labelsListing.count());
    ui->tableWidget_Listing->setHorizontalHeaderLabels(labelsListing);
    for(i=0;i<labelsListing.count();i++)
        hdrListing[labelsListing.at(i)]=i;
    connect(ui->pB_loadListing,SIGNAL(clicked()),this,SLOT(loadListing()));


    //Init des widgets gerant les resultats des arrivants
    ui->comboBox_ChkPoints->addItems(labelChkPtsList);

    //pour supprimer un coureur
    connect(ui->pB_delRacer,SIGNAL(clicked()),this,SLOT(delNumberRacer()));


    //option
    ui->cB_autoSave->setChecked(autoSaveEnable);



    connect(ui->lineEditFinish,SIGNAL(returnPressed()),this,SLOT(enterNumberRacer()));
    connect(ui->lineEditFinish,SIGNAL(returnPressed()),this,SLOT(takePicture()));
    connect(ui->pB_saveResults,SIGNAL(clicked()),this,SLOT(saveFinishersResults()));
    connect(ui->pB_printResults,SIGNAL(clicked()),this,SLOT(printFinishersResults()));
    connect(ui->pB_loadLastRace,SIGNAL(clicked()),this,SLOT(restoreStartedRace()));
#ifdef TEST
    testDone=false;
    testTimer = new QTimer(this);
    connect(this,SIGNAL(debug()),this, SLOT(enterNumberRacer()));
    connect(testTimer,SIGNAL(timeout()),this,SLOT(runTestStep()));
#endif

    //sauvegarde d'une palette de couleur de base
    normalPalette=ui->tableWidget_Listing->palette();

    //autosave at close
    connect(this, SIGNAL(autoSave(QString)),this,SLOT(saveResults(QString)));
    //autosave each amount of time
    connect(ui->cB_autoSave,SIGNAL(stateChanged(int)),this,SLOT(enableAutosave(int)));
    ui->spB_syncTime->setValue(syncTimeAutoSave);
    connect(ui->spB_syncTime,SIGNAL(valueChanged(int)),this,SLOT(setSyncTimeAutoSave(int)));


    //for capture racer bib
    viewfinder = new QCameraViewfinder(ui->preview);
    QVBoxLayout *layout_1=new QVBoxLayout();
    ui->preview->setLayout(layout_1);
    viewfinder->setMinimumSize(QSize(300,300));
    viewfinder->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Maximum);
    viewfinder->setAspectRatioMode(Qt::KeepAspectRatio);

    connect(ui->cB_camera,SIGNAL(stateChanged(int)),this,SLOT(enableCamera(int)));
    cameraEnabled=false;
    if(useCamera)
    {
        ui->cB_camera->setChecked(true);
    }

    //suivi live
    ui->tabRaceLive->setEnabled(false);

    connect(ui->pb_ChksPtsAdd,SIGNAL(clicked()),this, SLOT(addCheckPoint()));
    connect(ui->pb_ChksPtsDel,SIGNAL(clicked()),this, SLOT(deleteCheckPoint()));

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::loadListing()
{
    bool isLoaded=false;

    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Ouvrir Listing"), "./", tr("Listing (*.csv)"));
    if (!(fileName.isEmpty()))
        isLoaded=importListingFromCSV(fileName,ui->tableWidget_Listing);
    if (isLoaded)
    {
        initRaceCharacteristics(ui->tableWidget_Listing);
        ui->tableWidget_Listing->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tabWidgetActions->setTabEnabled(0,true);
        ui->startButton->setEnabled(true);
        ui->pB_loadLastRace->setEnabled(false);
        ui->pB_loadListing->setEnabled(false);
    }
}

void MainWindow::showTime()
{
    QString text=getTime(':');
    if(showExternTime)
        showExternTime->display(text);
    ui->raceTimerLCD->display(text);
}



void MainWindow::countTime()
{
    //every x min we save data
    if (autoSaveEnable)
        if(elapsedTime%(syncTimeAutoSave*60)==0)
            emit autoSave("sauvegardeAuto.csv");

    //every x min we resynchronize time with PC clock
    if(elapsedTime%(syncTime*60)==0)
        elapsedTime=initStart.secsTo(QDateTime::currentDateTime());
    else
        elapsedTime++;
}

void MainWindow::oneSecondeActions()
{
    countTime();
    showTime();
}

void MainWindow::startClicked()
{
    timer->start(1000);
    //si on ne repart pas d'un ancien chrono on sauvegarde l'heure de debut de course
    if(deltaTime==0)
        initStart=QDateTime::currentDateTime();

    ui->startButton->setDisabled(true);
    ui->stopButton->setDisabled(false);
    ui->lineEditFinish->setDisabled(false);

    ui->pB_loadListing->setDisabled(true);
    ui->pB_loadLastRace->setDisabled(true);

    //now we cannot choose option anymore
    ui->tabOption->setDisabled(true);
    //or add,delete another checkpoints
    ui->pb_ChksPtsAdd->setDisabled(true);
    ui->pb_ChksPtsDel->setDisabled(true);

    //on enregistre le nombre total de coureurs inscrits, toute course confondue
    countListingRacers=ui->tableWidget_Listing->rowCount();

    ui->tabRaceLive->setEnabled(true);
#ifdef TEST
    testTimer->start(200);
    qDebug() << "Test démarré";
#endif
}

void MainWindow::restoreStartedRace()
{
    //on recharge les anciens résultats

    bool isLoaded=false;
    int maxSubRaces = 0;
    QString fileName = QFileDialog::getOpenFileName(this,
        tr("Ouvrir Resultats"), "./", tr("Results (*.csv)"));
    if (!(fileName.isEmpty()))
    {
        isLoaded=importFromResultsCSV(fileName,false);
        maxSubRaces = subRaces.count();
    }


    //on calcul le temps a ajouter au chrono en se basant sur l'horaire
    //de depart sauvegarde
//    QTime presentTime=QTime::currentTime();
//    float diffTime=(savedStart.msecsTo(presentTime))/1000;
//    deltaTime=(int)diffTime;

    QDateTime presentDateTime=QDateTime::currentDateTime();
    float diffTime=initStart.secsTo(presentDateTime);
    deltaTime=(int)diffTime;

    //mise à jour du chrono
    elapsedTime=deltaTime;
    showTime();

    //pour un lancement automatique du chrono
    //startClicked();
    if (isLoaded)
    {
        //order the racers
        for(int idxSubRace=0;idxSubRace<maxSubRaces;idxSubRace++)
        {
            disconnect(subRacesResults.at(idxSubRace),SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
            int maxChkPts=numberOfChkPts[subRaces[idxSubRace]];

            subRacesResults[idxSubRace]->sortItems(hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(maxChkPts-1)));
            //check their results
            classRacers();
            connect(subRacesResults.at(idxSubRace),SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
        }

        ui->tableWidget_Listing->setEditTriggers(QAbstractItemView::NoEditTriggers);
        ui->tabWidgetActions->setTabEnabled(0,true);
        ui->startButton->setEnabled(true);
        ui->pB_loadLastRace->setEnabled(false);
        ui->pB_loadListing->setEnabled(false);
        //or add,delete another checkpoints
        ui->pb_ChksPtsAdd->setDisabled(true);
        ui->pb_ChksPtsDel->setDisabled(true);
    }
}

void MainWindow::stopClicked(){
    timer->stop();
}

bool MainWindow::importFromCSV(QString nameFile, QTableWidget* table, bool withRowHeader)
{
    bool atLeastOneLine=false;
    bool isHeader=withRowHeader;
    int i=0;
    QFile file(nameFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        int indexItem=table->rowCount();
        while (!file.atEnd())
        {
            QString line = file.readLine();
            line.remove(QRegExp("[\\n\\t\\r]"));
            if (isHeader)
                    isHeader=false;
            else
            {
                QStringList listingLine=processLineFromCSV(line);
                table->insertRow(indexItem);
                for(i=0;i<table->columnCount();i++)
                    table->setItem(indexItem,i,new QTableWidgetItem(listingLine.value(i)));

                indexItem++;
            }
        }

        if (indexItem>0)
            atLeastOneLine=true;

    file.close();
    }

    return atLeastOneLine;
}

bool MainWindow::importListingFromCSV(QString nameFile, QTableWidget* table)
{
    bool atLeastOneLine=false;
    bool isHeader=true;
    bool isGoodFile=true;
    int i=0;int j=0;
    QStringList listingLine;

    QFile file(nameFile);
    //bit field to determine if we have all the listing header in the file
    bool headerExistence[table->columnCount()];
    for(i=0;i<table->columnCount();i++)
        headerExistence[i]=false;
    int headerIndex[table->columnCount()];
    for(i=0;i<table->columnCount();i++)
        headerIndex[i]=0;
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        int indexItem=table->rowCount();
        while ((!file.atEnd())&&(isGoodFile))
        {
            QString line = file.readLine();
            line.remove(QRegExp("[\\n\\t\\r]"));
            if (isHeader)
            {
                isHeader=false;
                listingLine=processLineFromCSV(line);
                //verification of the validity of the file
                for(j=0;j<table->columnCount();j++)
                {
                    for(i=0;i<listingLine.count();i++)
                    {
                        if(listingLine.at(i).compare(table->horizontalHeaderItem(j)->text())==0)
                        {
                            headerExistence[j]=true;
                            headerIndex[j]=i;
                        }
                    }
                }
                listingLine.clear();
                for(j=0;j<table->columnCount();j++)
                    isGoodFile=isGoodFile && headerExistence[j];

                if (!isGoodFile)
                {
                    QMessageBox::information( this, "Importation de fichier",tr("Le fichier choisi n'est pas au bon format!\n"), QMessageBox::Ok,  QMessageBox::Ok);
                }
                /*else
                    qDebug() << "fichier correct";*/

            }
            else
            {
                listingLine=processLineFromCSV(line);
                table->insertRow(indexItem);
                for(i=0;i<table->columnCount();i++)
                    table->setItem(indexItem,i,new QTableWidgetItem(listingLine.value(headerIndex[i])));
                listingLine.clear();

                indexItem++;
            }
        }

        if ((indexItem>0)&&(isGoodFile))
            atLeastOneLine=true;

    file.close();
    }

    return atLeastOneLine;
}

bool MainWindow::importFromResultsCSV(QString nameFile, bool withRowHeader)
{
    bool atLeastOneLine=false;
    int nbLignes=0;
    bool isHeader=withRowHeader;
    int i=0;
    bool withResults=false;

    int maxCol=ui->tableWidget_Listing->columnCount();
    qDebug() << maxCol;

    QHash<QString, int> maxNumberOfChkPts;

    QFile file(nameFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        int indexItem=ui->tableWidget_Listing->rowCount();
        while (!file.atEnd())
        {
            QString line = file.readLine();
            line.remove(QRegExp("[\\n\\t\\r]"));
            indexItem=ui->tableWidget_Listing->rowCount();

            if (isHeader)
                    isHeader=false;
            else
            {
                QStringList listingLine=processLineFromCSV(line);
                nbLignes++;

                //on remplit le listing
                ui->tableWidget_Listing->insertRow(indexItem);
                for(i=0;i<maxCol;i++)
                {
                    ui->tableWidget_Listing->setItem(indexItem,i,new QTableWidgetItem(listingLine.value(i)));

                }
                int sumOfResults=0;//there are somme results if >0
                //considering that there is at last the result of the finish line
                //enter just the not null results
                if(listingLine.count()>maxCol)
                    for(i=maxCol;i<listingLine.count();i++)
                        if(!(listingLine.value(i).isEmpty()))
                            sumOfResults++;

                /*if(nbLignes<20){
                    //qDebug() << listingLine;
                    qDebug() <<nbLignes<<"\tresultats"<<sumOfResults << "\tenregistrements" << listingLine.count();
                }*/


                //TODO: ajout de 1 chekpoint??
                if(!(maxNumberOfChkPts.contains(listingLine.value(hdrListing[str_subRace]))))
                    if(sumOfResults<=0)
                        maxNumberOfChkPts[listingLine.value(hdrListing[str_subRace])]=1;
                    else
                        maxNumberOfChkPts[listingLine.value(hdrListing[str_subRace])]=sumOfResults;
                else
                    if(sumOfResults>maxNumberOfChkPts[listingLine.value(hdrListing[str_subRace])])
                        maxNumberOfChkPts[listingLine.value(hdrListing[str_subRace])]=sumOfResults;
            }
        }

        //on initialise la gui et les caractéristiques course
        initRaceCharacteristics(ui->tableWidget_Listing);

        //number of max checkpoints
        for(i=0;i<subRaces.count();i++)
        {
            //qDebug()<<maxNumberOfChkPts;
            numberOfChkPts[subRaces.at(i)]=maxNumberOfChkPts[subRaces.at(i)];
            if(numberOfChkPts[subRaces.at(i)]>1)
            {
                //we skip the checkpoint of the finish line
                for(int j=1;j<numberOfChkPts[subRaces.at(i)];j++)
                {
                    newCheckPoint(i,QString("CP_%1").arg(j));
                }

            }
        }

        //rewind the file
        file.seek(0);

        isHeader=withRowHeader;
        while (!file.atEnd())
        {
            QString line = file.readLine();
            line.remove(QRegExp("[\\n\\t\\r]"));

            if (isHeader)
                    isHeader=false;
            else
            {
                QStringList listingLine=processLineFromCSV(line);
                //for(i=maxCol;i<numberOfChkPts[listingLine.value(hdrListing[str_subRace])];i++)
                if(listingLine.count()>maxCol)
                {
                    withResults=false;

                    //a-t-on des resultats dans ces colonnes supplementaires
                    for(i=maxCol;i<listingLine.count();i++)
                        if(!(listingLine[i].isEmpty()))
                            withResults=true;

                    if (withResults)
                    {
                        int idxSubRace=subRaces.indexOf(listingLine.value(hdrListing[str_subRace]));
                        disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                        QString selRacer=listingLine.value(hdrListing[str_bib]);
                        //TODO verouiller l'index max
                        int maxResults2=maxCol+numberOfChkPts[subRaces.at(idxSubRace)];
                        int rowSel=0;
                        for(i=maxCol;i<maxResults2;i++)
                        {
                            QString addTime=listingLine[i];
                            if(!(addTime.isEmpty()))
                            {
                                int numChckPts=i-maxCol;
                                if(numChckPts==0)
                                {

                                    rowSel=addScore(idxSubRace,selRacer,addTime,numChckPts,-1);

                                    //on fini de remplir la ligne des resultats avec les infos manquantes
                                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_name))->setText(listingLine.value(hdrListing[str_name]));
                                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_frstname))->setText(listingLine.value(hdrListing[str_frstname]));
                                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_assoc))->setText(listingLine.value(hdrListing[str_assoc]));

                                    //qDebug()<<"ajout du chekpoint"<< numChckPts<<"à"<<addTime << "au coureur "<< selRacer <<"dans la course"<<subRaces.at(idxSubRace);
                                }
                                else
                                {
                                    addScore(idxSubRace,selRacer,addTime,numChckPts,rowSel);
                                    //qDebug()<<"ajout du chekpoint"<< numChckPts<<"à"<<addTime << "au coureur "<< selRacer <<"dans la course"<<subRaces.at(idxSubRace);
                                }
                            }

                        }
                        //TODO remplir les resultats
                        connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                    }
                }
                else
                {
                    withResults=false;
                }
            }
        }

        if (indexItem>0)
            atLeastOneLine=true;

    file.close();
    }

    return atLeastOneLine;

}

QStringList MainWindow::processLineFromCSV(QString line)
{
QStringList strings = line.split(";");
return strings;
}

int MainWindow::addScore(int idxSubRace,QString racerNumber,QString s_time, int numChkPts, int selRow)
{
    int i=0;
    //give the current index off the last racer added at the focused subrace
    int insertedInSubRace=countRacersList[idxSubRace];

    //selrow<0 means we want add a new racer to the list
    if(selRow<0)
    {
        //list of the widgets we want to append to the tablewidget
        QList<QTableWidgetItem*> ligneTableWidget;

        //create the ligne
        for(i=0;i<subRacesResults[idxSubRace]->columnCount();i++)
        {
            //it is the first score added for this racer, the first checkpoint
            if(i==hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace)[0]))
                ligneTableWidget.append(new QTableWidgetItem(s_time));
            //we add the bib
            else if (i==hdrResults.at(idxSubRace).indexOf(str_bib))
                ligneTableWidget.append(new QTableWidgetItem(racerNumber));

            //the other cells left empty
            else
            ligneTableWidget.append(new QTableWidgetItem());
        }

        //append the row
        subRacesResults[idxSubRace]->insertRow(insertedInSubRace);
        //fill the row with the widgets in the list
        for(i=0;i<subRacesResults[idxSubRace]->columnCount();i++)
            subRacesResults[idxSubRace]->setItem(insertedInSubRace, i, ligneTableWidget.at(i));

        //we return the index of the row (0 based), store the new last count of the list
        insertedInSubRace=countRacersList[idxSubRace];
        countRacersList[idxSubRace]=countRacersList[idxSubRace]+1;
    }
    //it's not a new racer to add, we modify the row by adding the new check point time
    else
    {
        subRacesResults[idxSubRace]->item(selRow,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(numChkPts)))->setText(s_time);
        insertedInSubRace=-1;
    }

    if(countRacersList[idxSubRace]==1)
    {
        ui->pB_saveResults->setDisabled(false);
        ui->pB_printResults->setDisabled(false);
        ui->pB_delRacer->setDisabled(false);
    }

    return insertedInSubRace;

}

void MainWindow::saveResults(QString fileName)
{
    QFile fichier2(fileName);
    int i,j,p;
    int idxSubRace=0;
    int const maxCol=hdrListing.count()+labelChkPts.at(idxSubRace).count();

    int countCol=0;
    // On ouvre notre fichier en lecture/ecriture seule et on vérifie l'ouverture
    if (fichier2.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        // Création d'un objet QTextStream à partir de notre objet QFile
        QTextStream flux2(&fichier2);
        // On choisit le codec correspondant au jeu de caractère que l'on souhaite ; ici, UTF-8
        flux2.setCodec("UTF-8");

        countCol=0;
        //on sauvegarde le listing et on complete avec les resultats
        for(p=0;p<ui->tableWidget_Listing->rowCount();p++)
        {
            QString resultLine;
            countCol=0;

            //which subrace for the racer
            idxSubRace=subRaces.indexOf(ui->tableWidget_Listing->item(p,hdrListing[str_subRace])->text());

            if(idxSubRace>=0) //if race not selected, skip the row
            {

                //save listing data first
                for(j=0;j<ui->tableWidget_Listing->columnCount();j++)
                {
                    resultLine.append(ui->tableWidget_Listing->item(p,j)->text());
                    resultLine.append(";");
                    countCol++;
                }

                //we are looking for the racer in the result tables
                for(i=0;i<subRacesResults[idxSubRace]->rowCount();i++)
                {
                    //find him/her
                    if(ui->tableWidget_Listing->item(p,hdrListing[str_bib])->text()==subRacesResults[idxSubRace]->item(i,hdrResults.at(idxSubRace).indexOf(str_bib))->text())
                    {
                        //we add results
                        for(j=hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace)[0]);j<subRacesResults[idxSubRace]->columnCount();j++)
                        {
                            resultLine.append(subRacesResults.at(idxSubRace)->item(i,j)->text());
                            resultLine.append(";");
                            countCol++;
                        }
                    }
                }

                //add blank results
                if(countCol<maxCol)
                    for(i=countCol;i<maxCol;i++)
                        resultLine.append(";");

                flux2 << resultLine << endl;
             }
        }
        fichier2.close();
    }
}

void MainWindow::closeEvent (QCloseEvent *event)
{
    QMessageBox::StandardButton resBtn=QMessageBox::Yes;

    if(timer->isActive())
        resBtn = QMessageBox::question( this, "Chrono",
                tr("Etes-vous sûr de vouloir fermer l'application?\nLe chrono sera arrêté!\n"),
                QMessageBox::No | QMessageBox::Yes,
                QMessageBox::Yes);
    qDebug() << "timer vérifié";

    if (resBtn != QMessageBox::Yes) {
        event->ignore();
    }
    else{
        writeConfig();
        stopClicked();
        if(camera)
        {
            camera->unlock();
            camera->stop();
        }
        showExternTime->close();
        if(!(subRacesResults.isEmpty()))
            {
                //Création d'un objet QFile
                QString fileName("resultats_");
                QString temps1 = QDate::currentDate().toString("yyyy_MM_dd_at_");
                QString temps2=QTime::currentTime().toString("hh_mm");
                fileName.append(temps1);
                fileName.append(temps2);
                fileName.append(".csv");
                saveResults(fileName);
            }
        event->accept();
    }
}

/*!
 * \brief MainWindow::modifyResults callback called when a cell at (rowSel, colSel) in results table is modified
 * \param rowSel
 * \param colSel
 */
void MainWindow::modifyResults(int rowSel, int colSel){
    int i;
    int j=0;
    int identifiedRow=0;
    int idxSubRace=subRaces.count()-1;
    int idxSubRaceChecked=subRaces.count()-1;
    int maxChkPts=1;

    //look for the pointer of the sender object in our table
    //to not cast the object later
    QObject* table = sender();
    if( table != NULL )
    {
        //and we determine the index of the subrace
        for(i=0;i<subRacesResults.count();i++)
        if(table==subRacesResults[i])
            idxSubRace=i;
    }

    //if we modify the cell containing the bib num
    if (colSel==hdrResults.at(idxSubRace).indexOf(str_bib))
    {
        //store the bib num
        QString selRacer=subRacesResults[idxSubRace]->item(rowSel,colSel)->text();

        //init default: verification are not done
        bool isChecked=false;

        //we have something to analyse :-)
        if ((selRacer.isEmpty())==false)
        {
            isChecked=true;

            //a quelle course appartient-il
            //pour l'instant il est stocke au dernier index de course
            idxSubRaceChecked=inWhichSubRace(selRacer);


            //recherche de donnée similaire
            bool ok;
            //int number=selRacer.toInt(&ok,10);
            selRacer.toInt(&ok,10);
            //est-ce que le dossard ressemble a un chiffre
            if(ok)
            {
                maxChkPts=numberOfChkPts[subRaces[idxSubRaceChecked]];
                //on recherche dans la liste de la course a laquelle le coureur appartient pour savoir
                //si on a deja saisi son numero de dossard
                for(i=0; i<subRacesResults[idxSubRaceChecked]->rowCount();i++)
                {
                    //attention a ne pas rechercher le meme num de dossard sur la ligne qu'on est en train
                    //de modifier
                    bool exceptedRow=(idxSubRaceChecked==idxSubRace)&&(i==rowSel);

                    //dossard deja saisi
                    if((!exceptedRow)&&(selRacer==subRacesResults[idxSubRaceChecked]->item(i,colSel)->text()))
                    {
                        //known bib: we store the row
                        int knownBibRow=i;
                        //verification flag, not rigth information yet
                        isChecked=false;

                        int ret=QMessageBox::question(this, "Chrono", tr("Dossard déjà saisi!\nAjouter le temps?\nLe temps de finsiher peut être modifié!"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

                        //modification callback off for the identified results table
                        disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));

                        //we accept the modification
                        if(ret==QMessageBox::Yes)
                        {
                            //gestion differente si les courses (celle ou le dossard a été enregistré et celle où il va etre transféré) ne sont pas les memes
                            if(idxSubRaceChecked!=idxSubRace)
                            {
                                //on desactive egalement la surveilance des modifs pour la course de destination
                                disconnect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));

                                //on met en avant la course qu'on va modifier
                                ui->tabResults->setCurrentIndex(idxSubRaceChecked);

                                //ATTENTION: le nombre de checkpoints peut différer selon la course

                                //Nombre de checkpoints passes par ce dossard sur la course de depart
                                maxChkPts=numberOfChkPts[subRaces[idxSubRace]];
                                int lastChkPtsInit=numberOfChkPts[subRaces[idxSubRace]]-1;
                                for(j=0;j<numberOfChkPts[subRaces[idxSubRace]];j++)
                                    //temps nul=checkpoint non passé et donc à remplir
                                    if(subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(j)))->text().isEmpty()==true)
                                        lastChkPtsInit=j;
                                //Nombre de checkpoints devant etre passes par ce dossard
                                if(lastChkPtsInit>maxChkPts)
                                    lastChkPtsInit=maxChkPts-1;

                                //Nombre de checkpoints passes par ce dossard sur la course de destination
                                maxChkPts=numberOfChkPts[subRaces[idxSubRaceChecked]];
                                int lastChkPts=numberOfChkPts[subRaces[idxSubRaceChecked]]-1;

                                for(j=numberOfChkPts[subRaces[idxSubRaceChecked]]-1;j>=0;j--)
                                    //temps nul=checkpoint non passé et donc à remplir
                                    if(subRacesResults[idxSubRaceChecked]->item(knownBibRow,hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRaceChecked).at(j)))->text().isEmpty()==true)
                                        lastChkPts=j;

                                //Nombre de checkpoints devant etre passes par ce dossard
                                if(lastChkPts>maxChkPts)
                                    lastChkPts=maxChkPts-1;


                                QString timeToAdd=subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(0)))->text();
                                subRacesResults[idxSubRaceChecked]->item(knownBibRow,hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRaceChecked).at(lastChkPts)))->setText(timeToAdd);
                                subRacesResults[idxSubRace]->removeRow(rowSel);
                                countRacersList[idxSubRace]=countRacersList[idxSubRace]-1;

                                //on reactive la surveillance
                                connect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                            }
                            else
                            {
                                //on met en avant la course qu'on va modifier
                                ui->tabResults->setCurrentIndex(idxSubRaceChecked);

                                //Nombre de checkpoints passes par ce dossard sur la course de destination
                                maxChkPts=numberOfChkPts[subRaces[idxSubRaceChecked]];
                                int lastChkPts=numberOfChkPts[subRaces[idxSubRaceChecked]]-1;
                                for(j=numberOfChkPts[subRaces[idxSubRaceChecked]]-1;j>=0;j--)
                                    //temps nul=checkpoint non passé et donc à remplir
                                    if(subRacesResults[idxSubRaceChecked]->item(knownBibRow,hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRace).at(j)))->text().isEmpty()==true)
                                        lastChkPts=j;
                                //Nombre de checkpoints devant etre passes par ce dossard
                                if(lastChkPts>maxChkPts)
                                    lastChkPts=maxChkPts-1;

                                QString timeToAdd=subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(0)))->text();
                                subRacesResults[idxSubRaceChecked]->item(knownBibRow,hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRaceChecked).at(lastChkPts)))->setText(timeToAdd);
                                subRacesResults[idxSubRace]->removeRow(rowSel);
                                countRacersList[idxSubRace]=countRacersList[idxSubRace]-1;
                            }

                        }
                        //on ne fait rien (on s'est trompe de dossard)
                        else
                            subRacesResults[idxSubRace]->item(rowSel,colSel)->setText("");

                        connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));

                    }
                }
                disconnect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                subRacesResults[idxSubRaceChecked]->sortItems(hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRaceChecked).at(maxChkPts-1)));
                classRacers();
                connect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
            }
            else
            {
                isChecked=false;
                QMessageBox::information( this, "Chrono",tr("Format de saisie invalide\n"), QMessageBox::Ok,  QMessageBox::Ok);
                disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                subRacesResults[idxSubRace]->item(rowSel,colSel)->setText("");
                connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
            }
         }

       //On a un numero de dossard, au bon format et jamais saisi
       //il doit etre enregistrer dans la bonne course
       if (isChecked)
       {


           //we check the bib in the listing
          identifiedRow=rowInListing(selRacer);


           int ret=QMessageBox::Yes;
           //unknown bib in the listing, do we change?
           if((identifiedRow<0)&&(!(selRacer.isEmpty())))
           {
               QString QS_message="Dossard " +selRacer+" inconnu\nModifier tout de même?";
               ret=QMessageBox::question(this, "Chrono", QS_message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
           }

            if(ret==QMessageBox::Yes)
            {

               //a quelle course appartient-il
               //pour l'instant il est stocke au premier index de course
               idxSubRaceChecked=inWhichSubRace(selRacer);



               ui->tabResults->setCurrentIndex(idxSubRaceChecked);

               if(idxSubRace==idxSubRaceChecked) //je suis dans le bon tableau de resultat
               {
                    //on fini de remplir la ligne des resultats avec les infos manquantes
                   if(identifiedRow>=0)
                   {
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_name))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_name])->text());
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_frstname))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_frstname])->text());
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_assoc))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_assoc])->text());
                   }
                   else
                   {
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_name))->setText("?");
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_frstname))->setText("?");
                    subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_assoc))->setText("?");
                   }
               }
               else //je le renseigne dans un autre tableau et je le retire du tableau present
               {
                   QString timeToAdd=subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(0)))->text();
                   disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                   disconnect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                   int newRowSel=addScore(idxSubRaceChecked,selRacer,timeToAdd,0,-1);
                   //on fini de remplir la ligne des resultats avec les infos manquantes
                  if(identifiedRow>=0)
                  {
                       //on remplit la ligne des résultats avec les infos manquantes
                       subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_name))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_name])->text());
                       subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_frstname))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_frstname])->text());
                       subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_assoc))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_assoc])->text());
                  }
                  else
                  {
                      //on remplit la ligne des résultats avec les infos manquantes
                      subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_name))->setText("?");
                      subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_frstname))->setText("?");
                      subRacesResults[idxSubRaceChecked]->item(newRowSel,hdrResults.at(idxSubRaceChecked).indexOf(str_assoc))->setText("?");
                  }
                   subRacesResults[idxSubRace]->removeRow(rowSel);
                   countRacersList[idxSubRace]=countRacersList[idxSubRace]-1;
                   subRacesResults[idxSubRaceChecked]->sortItems(hdrResults.at(idxSubRaceChecked).indexOf(labelChkPts.at(idxSubRaceChecked).at(maxChkPts-1)));
                   connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                   connect(subRacesResults[idxSubRaceChecked],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
               }
            }

       }
    }

    //Quelle que soit la modif on trie les resultats et on refait le classement
    for(i=0;i<subRacesResults.count();i++)
    {
        disconnect(subRacesResults[i],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
        maxChkPts=numberOfChkPts[subRaces[i]];
        subRacesResults[i]->sortItems(hdrResults.at(i).indexOf(labelChkPts.at(i).at(maxChkPts-1)));
        classRacers();
        connect(subRacesResults[i],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
    }

    //set the focus: ready to enter a new bib num
    ui->lineEditFinish->setFocus();

}

void MainWindow::delNumberRacer()
{
    int idxSubRace=0;
    //we are looking for the index of the subrace which has the focus
    idxSubRace=ui->tabResults->currentIndex();
    //bib to delete (bib selected)
    int idxRowRacer=subRacesResults[idxSubRace]->currentRow();

    if(idxRowRacer>=0)
    {
        QString stringBib=subRacesResults[idxSubRace]->item(idxRowRacer,hdrResults.at(idxSubRace).indexOf(str_bib))->text();

        QString QS_message="Etes vous sûr de vouloir effacer le dossard " +stringBib+"\net toutes ses données de temps.";
        int ret=QMessageBox::question(this, "Chrono", QS_message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        //on supprime la ligne
        if(ret==QMessageBox::Yes)
        {
            int maxChkPts=numberOfChkPts[subRaces[idxSubRace]];
            disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
            subRacesResults[idxSubRace]->removeRow(idxRowRacer);
            countRacersList[idxSubRace]=countRacersList[idxSubRace]-1;
            subRacesResults[idxSubRace]->sortItems(hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(maxChkPts-1)));
            classRacers();
            connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
        }
    }
}

void MainWindow::enterNumberRacer(){
    int i,j;
    int idxSubRace=subRaces.count()-1;
    QString selRacer=ui->lineEditFinish->text();
    int identifiedRow=0;

        bool isChecked=false;
        int countChkPts=0; //Nombre de checkpoints passes par ce dossard
        int maxChkPts=1; //nombre de checkpoints devant etre passes par ce dossard
        int newTimeRow=-1;
        int rowKnownNumRacer=-1;
        //si le champs n'est pas vide
        if ((selRacer.isEmpty())==false)
        {
            isChecked=true;
            bool ok;
            //int number=selRacer.toInt(&ok,10);
            selRacer.toInt(&ok,10);

            //Bon format de saisie
            if(ok)
            {
                //quelle course
                idxSubRace=inWhichSubRace(selRacer);
                maxChkPts=numberOfChkPts[subRaces[idxSubRace]];


                //recherche de donnees similaires
                for(i=0; i<subRacesResults[idxSubRace]->rowCount();i++)
                {
                   if(selRacer.compare(subRacesResults[idxSubRace]->item(i,hdrResults.at(idxSubRace).indexOf(str_bib))->text())==0)
                       rowKnownNumRacer=i;
                }

                //dossard existant dans la liste des resultats
                if(rowKnownNumRacer>=0)
                {
                    //Nombre de checkpoints passes par ce dossard
                    countChkPts=0;

                    for(j=0; j<maxChkPts;j++)
                        //temps non nul=checkpoint déjà passé, on retient son rang
                        if(subRacesResults[idxSubRace]->item(rowKnownNumRacer,hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(j)))->text().isEmpty()==false)
                            countChkPts++;

                    //Nombre de checkpoints devant etre passes par ce dossard
                   identifiedRow=rowInListing(selRacer);

                    //ts les checkpoints possibles sont saisis, c'est un finisher, c'est sur :-)
                    if((countChkPts>=labelChkPts.at(idxSubRace).count())||(countChkPts>=maxChkPts))
                    {
                        QMessageBox::information( this, "Chrono",tr("Ce coureur est déjà arrivé\n"), QMessageBox::Ok,  QMessageBox::Ok);
                        isChecked=false;
                    }
                    //il reste des checkpoints a passer avant de terminer
                    if ((countChkPts<maxChkPts) && (countChkPts>0))
                    {

                        QString QS_message="Dossard déjà passé au" +labelChkPts.at(idxSubRace)[countChkPts-1]+"\nAjouter ce temps?";
#ifdef TEST
                        int ret=QMessageBox::Yes;
#else
                        int ret=QMessageBox::question(this, "Chrono", QS_message, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
#endif
                        //on inscrit le temps sur ce numero de dossard
                        if(ret==QMessageBox::Yes)
                        {
                            isChecked=true;
                            newTimeRow=rowKnownNumRacer;
                        }
                        //on ne fait rien (on s'est trompe de dossard)
                        else
                            isChecked=false;
                    }

                    //isChecked=false;
                    ui->lineEditFinish->clear();
                }
                //nouveau dossard
                else
                {
                    isChecked=true;
                    //ui->lineEditFinish->clear();
                }

            }
            //Mauvais format de saisie
            else
            {
                QMessageBox::information( this, "Chrono",tr("Format de saisie invalide\n"), QMessageBox::Ok,  QMessageBox::Ok);
                isChecked=false;
                //ui->lineEditFinish->clear();
            }
       }
       else
       {
            isChecked=true; //champ vide on ajoute un coureur sans dossard
            idxSubRace=subRaces.count()-1;
        }

       //si la saisie est correcte on ajoute les informations
       if (isChecked)
       {
           //we check the bib in the listing
           identifiedRow=-1;
           if(!(selRacer.isEmpty()))
           {
               identifiedRow=rowInListing(selRacer);
           }

           int ret=QMessageBox::Yes;
           //unknown bib in the listing, do we add?
           if((identifiedRow<0)&&(!(selRacer.isEmpty()))&& (rowKnownNumRacer<0))
           {
               QString QS_message="Dossard " +selRacer+" inconnu\nAjouter tout de même ce temps?";
               ret=QMessageBox::question(this, "Chrono", QS_message, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
           }

           if(ret==QMessageBox::Yes)
           {
                disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
                ui->tabResults->setCurrentIndex(idxSubRace);

                //manage state of the chrono
                QString s_time;
                if ((elapsedTime>0)&&(timer->isActive())) //Chrono already started
                    s_time=getTime(':');
                else
                    s_time="Non Classe";

                //add score to the table
                int rowSel=addScore(idxSubRace,selRacer,s_time,countChkPts,newTimeRow);

                //complete with racer's information
                if(rowSel>=0)
                {
                    //unknown or "blank" bib
                    if(identifiedRow<0)
                    {
                        subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_name))->setText("?");
                        subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_frstname))->setText("?");
                    }
                    else
                    {
                        subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_name))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_name])->text());
                        subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_frstname))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_frstname])->text());
                        subRacesResults[idxSubRace]->item(rowSel,hdrResults.at(idxSubRace).indexOf(str_assoc))->setText(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_assoc])->text());
                    }
                }

                //order the racers
                maxChkPts=numberOfChkPts[subRaces[idxSubRace]];

                /*if(maxChkPts==0)
                    subRacesResults[idxSubRace]->sortItems(hdrResults[labelChkPts.at(maxChkPts)]);
                else*/
                    subRacesResults[idxSubRace]->sortItems(hdrResults.at(idxSubRace).indexOf(labelChkPts.at(idxSubRace).at(maxChkPts-1)));
                //check their results
                classRacers();

                connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
           }
           //pret pour une nouvelle saisie
           ui->lineEditFinish->clear();

       }

   //Quelle que soit la modif on trie les resultats et on refait le classement
   for(i=0;i<subRacesResults.count();i++)
   {
       disconnect(subRacesResults[i],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
       maxChkPts=numberOfChkPts[subRaces[i]];
       /*if(maxChkPts==0)
        subRacesResults[i]->sortItems(hdrResults[labelChkPts.at(maxChkPts)]);
       else*/
           subRacesResults[i]->sortItems(hdrResults.at(i).indexOf(labelChkPts.at(i).at(maxChkPts-1)));
       classRacers();
       connect(subRacesResults[i],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
   }
}

void MainWindow::takePicture()
{
    bool b_takePicture=false;

    //manage state of the chrono
    QString s_time;
    if ((elapsedTime>0)&&(timer->isActive())) //chrono already started
    {
        s_time = getTime('_');
        b_takePicture=true;
    }

    if ((b_takePicture)&&(cameraEnabled))
    {
        QString namefile=s_time+".jpg";
        imageCapture->capture(namefile);
    }
}
#ifdef TEST
void MainWindow::runTestStep()
{
    if((testBids.size()==0)&&(!testDone))
    {
        //init vector
        int listingSize= ui->tableWidget_Listing->rowCount();
        int idxSubRace=0;
        QString selRacer;
        int bibCol=hdrListing[str_bib];
        int maxChkPts=1;

        for(int i=0;i<listingSize;i++)
        {
            selRacer=ui->tableWidget_Listing->item(i,bibCol)->text();
            idxSubRace=inWhichSubRace(selRacer);
            maxChkPts=numberOfChkPts[subRaces[idxSubRace]];
            for(int j=0;j<maxChkPts;j++)
                testBids.append(selRacer);
            if (maxChkPts<=1)
                qDebug()<<"13km";
            else
                qDebug()<<"26km";
        }
        qDebug()<<"init du vecteur de test terminé";
    }
    else
    {
        //write bids and enter
        //QString selRacer=ui->lineEditFinish->text();
        if(testBids.size()>1)
        {
            int Low=0;
            int High=testBids.size()-1;
            qsrand(qrand());
            int idxToAdd=(qrand() % ((High + 1) - Low) + Low);
            QString selRacer=testBids.at(idxToAdd);
            ui->lineEditFinish->setText(selRacer);
            ui->lineEditFinish->returnPressed();
            //qDebug() << "ajout du dossard "<<selRacer<<" à l'index "<<idxToAdd;
            testBids.removeAt(idxToAdd);
        }
        else
            if(!testDone){
                ui->lineEditFinish->setText(testBids.at(0));
                ui->lineEditFinish->returnPressed();
                testDone=true;
                qDebug() << "Test terminé";
            }

    }
}
#endif

void MainWindow::saveFinishersResults(){
    QString fileName = QFileDialog::getSaveFileName(this, tr("Enregistrer les résultats"),
                               "./Resultats.csv",
                               tr("Fichier CSV (*.csv)"));
    saveResults(fileName);
}


void MainWindow::printFinishersResults(){

    QString fileHTML;
    QString filePDF;
    QString absoluteFileName=QDir::currentPath();
    QString fileName("resultats_");
    QString temps1 = QDate::currentDate().toString("yyyy_MM_dd_at_");
    QString temps2=QTime::currentTime().toString("hh_mm");

    //fileHTML=absoluteFileName+"/"+fileName+temps1+temps2+".html";
    fileHTML=absoluteFileName+"/"+fileName+".html";
    filePDF=fileName+temps1+temps2+".pdf";

    printResultsHTML(fileHTML);
    //QDesktopServices::openUrl(QUrl(fileHTML, QUrl::TolerantMode));
    printResultsPDF(filePDF);
}

void MainWindow::printResultsHTML(QString fileName){
    int i,j;
    QFile file(fileName);
    // On ouvre notre fichier en lecture seule et on vérifie l'ouverture
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        // Création d'un objet QTextStream à partir de notre objet QFile
        QTextStream flux(&file);
        // On choisit le codec correspondant au jeu de caractère que l'on souhaite ; ici, UTF-8
        flux.setCodec("UTF-8");
        //creation de l'entete
        flux << "<!DOCTYPE html>\n<html>\n\t<head>\n\t<meta charset=\"utf-8\" />\n\t\t<title>Resultats</title>\n\t</head>\n\t<body>\n";
        for(int idxSubRace=0; idxSubRace<subRaces.count();idxSubRace++)
        {
            flux << "\t\t<p>"+subRaces[idxSubRace]+"</p>\n";
            flux << "\t\t<table border=\"1\" align=\"center\">\n";
            //entete
            flux << "\t\t\t<thead>\n\t\t\t\t<tr>\n";
            flux << "\t\t\t\t<th>" << "Scratch" << "</th>\n";
            for(i=0; i<subRacesResults.at(idxSubRace)->columnCount();i++)
            {
                flux << "\t\t\t\t<th>" << subRacesResults.at(idxSubRace)->horizontalHeaderItem(i)->text() << "</th>\n";
            }
            flux << "\t\t\t\t</tr>\n\t\t\t</head>\n";

            //lignes
            for (i=0;i<(subRacesResults.at(idxSubRace)->rowCount());i++)
            {
                flux << "\t\t\t<tr>\n";
                flux << "\t\t\t\t<td>" << i << "</td>\n";
                for(j=0;j<subRacesResults.at(idxSubRace)->columnCount();j++)
                    flux << "\t\t\t\t<td>" << subRacesResults.at(idxSubRace)->item(i,j)->text() << "</td>\n";
                flux << "\t\t\t</tr>\n";
            }
            flux << "\t\t</table>\n";
        }
        flux<<"</body>\n</html>";
        file.close();
    }
}

void MainWindow::printResultsPDF(QString fileName)
{
    QTextDocument textDocument;
    //textDocument.addResource(QTextDocument::ImageResource,);

    QTextCursor cursor(&textDocument);

    QTextImageFormat logo;
    logo.setName("./logoRace.png");
    logo.setWidth(61);
    cursor.insertImage(logo,QTextFrameFormat::InFlow);

    QString title="\nRésultats de "+raceName+QDate::currentDate().toString(" dd MM yyyy")+"\n\n";

    cursor.insertText(title);

    for(int idxSubRace=0;idxSubRace<subRaces.count();idxSubRace++)
    {
//        if(idxSubRace>0)
//        {
//            QTextBlockFormat blockFormat;
//            blockFormat.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
//            cursor.insertBlock(blockFormat);
//        }
        QString tableTitle="\nRésultats de "+subRaces[idxSubRace]+"\n\n";
        cursor.insertText(tableTitle);
        addTable(cursor,idxSubRace);
    }

//    QTextBlockFormat blockFormat;
//    blockFormat.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
//    cursor.insertBlock(blockFormat);
//    cursor.insertText("This is the second page");

    QPrinter printer;
    printer.setOutputFileName(fileName);
    printer.setFullPage(true);

    printDocument(printer, &textDocument, 0);
}

void MainWindow::readConfig(void)
{
    // Ouverture du fichier INI
    QSettings settings("./XTremQhrono.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    QString byDefaultS;/*("The Race");
    raceName=settings.value("race/name",byDefaultS).toString();
    byDefaultS.clear();*/

    byDefaultS="Dossard";
    str_bib=settings.value("listing/str_bib",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Nom";
    str_name=settings.value("listing/str_name",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Prénom";
    str_frstname=settings.value("listing/str_frstname",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Classement";
    str_order=settings.value("listing/str_order",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Distance choisie";
    str_subRace=settings.value("listing/str_subRace",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Nom Club";
    str_assoc=settings.value("listing/str_assoc",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Sexe";
    str_genre=settings.value("listing/str_genre",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Scratch";
    str_scratch=settings.value("listing/str_scratch",byDefaultS).toString();
    byDefaultS.clear();
    byDefaultS="Temps";
    str_time=settings.value("listing/str_time",byDefaultS).toString();
    byDefaultS.clear();


    byDefaultS="The Race";
    raceName=settings.value("race/race_name",byDefaultS).toString();
    byDefaultS.clear();

    QStringList byDefaultQS;
    //Init des checkpoints
    labelChkPtsList=settings.value("race/checkPoints", byDefaultQS).toStringList();
    if(labelChkPtsList.isEmpty())
        labelChkPtsList << "--";
    else
        labelChkPtsList.insert(0,"--");


    byDefaultQS.clear();

    //au cas ou l'application a été fermée sans arrêt du chrono
    //savedStart=settings.value("race/savedStart").value<QTime>();
    initStart=settings.value("race/initStart").value<QDateTime>();

    //gestion des sauvegarde auto et de la synchro du temps
    int byDefaultI=1;
    syncTime=settings.value("option/synctime",byDefaultI).toInt();
    syncTimeAutoSave=settings.value("option/synctimeautosave",byDefaultI).toInt();
    bool byDefaultB=true;
    autoSaveEnable=settings.value("option/autosave",byDefaultB).toBool();

    byDefaultB=false;
    useCamera=settings.value("option/usecamera",byDefaultB).toBool();
}
void MainWindow::writeConfig(void)
{
    // Ouverture du fichier INI
    QSettings settings("./XTremQhrono.ini", QSettings::IniFormat);
    settings.setIniCodec(QTextCodec::codecForName("UTF-8"));

    //settings.setValue("race/savedStart",savedStart);
    settings.setValue("race/initStart",initStart);
}

int MainWindow::rowInListing(QString numRacer)
{
    int i;
    int searchRow=0;
    int identifiedRow=-1;

    searchRow=hdrListing[str_bib];
    //on recherche le dossard saisi dans le listing
    for(i=0;i<countListingRacers;i++)
        if(numRacer.compare(ui->tableWidget_Listing->item(i,searchRow)->text())==0)
        {
            identifiedRow=i;
            break;
        }
    return identifiedRow;
}

int MainWindow::inWhichSubRace(QString numRacer)
{
    int i,j;

    int identifiedSubRace=subRaces.count()-1;

    int searchCol=hdrListing[str_bib];
    int raceCol=hdrListing[str_subRace];
    //on recherche le dossard saisi dans le listing
    for(i=0;i<countListingRacers;i++)
        if(numRacer.compare(ui->tableWidget_Listing->item(i,searchCol)->text())==0)
        {
            for(j=0;j<subRaces.count();j++)
            {
//                QString subRaceReference=subRaces[j];
//                QString subRaceRead=ui->tableWidget_Listing->item(i,raceCol)->text();

                if(subRaces[j]==ui->tableWidget_Listing->item(i,raceCol)->text())
                {
                        identifiedSubRace=j;
                }
            }
            break;
        }
    return identifiedSubRace;
}

double MainWindow::mmToPixels(QPrinter& printer, int mm)
{
    return mm * 0.039370147 * printer.resolution();
}

void MainWindow::paintPage(QPrinter& printer, int pageNumber, int pageCount,
                      QPainter* painter, QTextDocument* doc,
                      const QRectF& textRect, qreal footerHeight)
{
    const QSizeF pageSize = printer.paperRect().size();

    const double bm = mmToPixels(printer, borderMargins);
    const QRectF borderRect(bm, bm, pageSize.width() - 2 * bm, pageSize.height() - 2 * bm);
    painter->drawRect(borderRect);

    painter->save();
    // textPageRect is the rectangle in the coordinate system of the QTextDocument, in pixels,
    // and starting at (0,0) for the first page. Second page is at y=doc->pageSize().height().
    const QRectF textPageRect(0, pageNumber * doc->pageSize().height(), doc->pageSize().width(), doc->pageSize().height());
    // Clip the drawing so that the text of the other pages doesn't appear in the margins
    painter->setClipRect(textRect);
    // Translate so that 0,0 is now the page corner
    painter->translate(0, -textPageRect.top());
    // Translate so that 0,0 is the text rect corner
    painter->translate(textRect.left(), textRect.top());
    doc->drawContents(painter);
    painter->restore();

    // Footer: page number or "end"
    QRectF footerRect = textRect;
    footerRect.setTop(textRect.bottom());
    footerRect.setHeight(footerHeight);
    if (pageNumber == pageCount - 1)
        painter->drawText(footerRect, Qt::AlignCenter, QObject::tr("Fin des Résultats"));
    else
        painter->drawText(footerRect, Qt::AlignVCenter | Qt::AlignRight, QObject::tr("Page %1/%2").arg(pageNumber+1).arg(pageCount));
}

void MainWindow::printDocument(QPrinter& printer, QTextDocument* doc, QWidget* parentWidget)
{
    QPainter painter( &printer );
    QSizeF pageSize = printer.pageRect().size(); // page size in pixels
    // Calculate the rectangle where to lay out the text
    const double tm = mmToPixels(printer, textMargins);
    const qreal footerHeight = painter.fontMetrics().height();
    const QRectF textRect(tm, tm, pageSize.width() - 2 * tm, pageSize.height() - 2 * tm - footerHeight);
    doc->setPageSize(textRect.size());

    const int pageCount = doc->pageCount();
    QProgressDialog dialog( QObject::tr( "Printing" ), QObject::tr( "Cancel" ), 0, pageCount, parentWidget );
    dialog.setWindowModality( Qt::ApplicationModal );

    bool firstPage = true;
    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex) {
        dialog.setValue( pageIndex );
        if (dialog.wasCanceled())
             break;

        if (!firstPage)
            printer.newPage();

        paintPage( printer, pageIndex, pageCount, &painter, doc, textRect, footerHeight );
        firstPage = false;
    }
}

void MainWindow::addTable(QTextCursor& cursor, int idxSubRace)
{
    const int columns = subRacesResults.at(idxSubRace)->columnCount();
    int maxCol=columns-1;
    const int rows = subRacesResults.at(idxSubRace)->rowCount();

    QTextTableFormat tableFormat;
    tableFormat.setHeaderRowCount( 1 );
    tableFormat.setCellSpacing(0);
    //tableFormat.setPageBreakPolicy(tableFormat.pageBreakPolicy() & QTextFormat::PageBreak_AlwaysAfter);
    tableFormat.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysAfter);
    int row, column;
    //on exclut des resultats les lignes des non-finishers, on marque
    //donc l'index de la premiere ligne avec un resultat final non nul
    int beginingRow=0;
    for(row=0;row<rows;row++)
        if(!(subRacesResults.at(idxSubRace)->item(row,maxCol)->text().isEmpty()))
        {
            beginingRow=row;
            break;
        }
    QTextTable* textTable = cursor.insertTable( (rows-beginingRow) + 1,
                                                columns,
                                                tableFormat );
    QTextCharFormat tableHeaderFormat;
    tableHeaderFormat.setBackground( QColor( "#DADADA" ) );

    for( column = 0; column < columns; column++ ) {
        QTextTableCell cell = textTable->cellAt( 0, column );
        Q_ASSERT( cell.isValid() );
        cell.setFormat( tableHeaderFormat );
        QTextCursor cellCursor = cell.firstCursorPosition();
        cellCursor.insertText( hdrResults.at(idxSubRace).at(column) );
    }


    for(row=beginingRow; row < rows; row++)
    {
        for( column = 0; column < columns; column++ )
        {
                QTextTableCell cell = textTable->cellAt( row-beginingRow+1, column );
                Q_ASSERT( cell.isValid() );
                QTextCursor cellCursor = cell.firstCursorPosition();
                const QString cellText = subRacesResults.at(idxSubRace)->item(row,column)->text();
                cellCursor.insertText( cellText );
        }
    }
    cursor.movePosition( QTextCursor::End );
}
/*
void MainWindow::setClassement(QString race)
{
    int i,j;
    int countChkPts=0;
    int maxChkPts=0;
    int identifiedRow=0;
    QHash<QString, QTime> finisherTime;

    //on parcourt le tableau de resultats
    for(i=0;i<subRacesResults.at(1)->rowCount();i++)
    {
        QString numRacer=subRacesResults.at(1)->item(i,hdrResults[str_bib])->text();
        identifiedRow=rowInListing(numRacer);

        //Nombre de checkpoints passes par ce dossard
        countChkPts=0;
        for(j=0;j<labelChkPts.count();j++)
            //temps non nul=checkpoint déjà passé, on incrémente le compteur
            if(subRacesResults.at(1)->item(i,hdrResults[labelChkPts.at(j)])->text().isEmpty()==false)
                countChkPts++;

        //Nombre de checkpoints devant etre passes par ce dossard
        maxChkPts=0;
        identifiedRow=rowInListing(numRacer);
        for(j=0;j<labelChkPts.count();j++)
            if(ui->tableWidget_Listing->item(identifiedRow,hdrListing[str_subRace])->text().compare(labelChkPts.at(j))==0)
            {
                maxChkPts=j+1;
            }


        //ts les checkpoints possibles sont saisis, c'est un finisher, c'est sur :-)
        if((countChkPts>=labelChkPts.count())||(countChkPts>=maxChkPts))
        {
            QMessageBox::information( this, "Chrono",tr("Ce coureur est déjà arrivé\n"), QMessageBox::Ok,  QMessageBox::Ok);
            isChecked=false;
        }
    }
}*/
void MainWindow::classRacers(void)
{
    int i,idxSubRace,rankM,rankF,totalRanked;

    totalRanked=1;

    for(idxSubRace=0;idxSubRace<subRaces.count();idxSubRace++)
    {
        int maxCol=subRacesResults[idxSubRace]->columnCount()-1;
        totalRanked=1;
        rankM=1;
        rankF=1;
        //On cherche dans la liste des resultats tries, le premier temps non nul pour commencer le classement
        for(i=0; i<subRacesResults[idxSubRace]->rowCount();i++)
            if(!(subRacesResults[idxSubRace]->item(i,maxCol)->text().isEmpty()))
            {
                int selRow=rowInListing(subRacesResults[idxSubRace]->item(i,hdrResults.at(idxSubRace).indexOf(str_bib))->text());
                QString str;
                QString str_rank;
                if (selRow>=0)
                {
                    if(ui->tableWidget_Listing->item(selRow,hdrListing[str_genre])->text()=="F")
                    {
                        str.setNum(rankF);
                        str_rank=str+"F";
                        rankF++;
                    }
                    else
                    {
                        str.setNum(rankM);
                        str_rank=str+"M";
                        rankM++;
                    }
                }
                else
                {
                    str.setNum(rankM);
                    str_rank=str+"M";
                    rankM++;
                }

                subRacesResults[idxSubRace]->item(i,hdrResults.at(idxSubRace).indexOf(str_scratch))->setText(str.setNum(totalRanked));
                subRacesResults[idxSubRace]->item(i,hdrResults.at(idxSubRace).indexOf(str_order))->setText(str_rank);

                totalRanked++;
            }
    }

    updateLive(totalRanked);

}

void MainWindow::setCamera(int numDevice) //const QByteArray &cameraDevice)
{
    if (imageCapture)
    delete imageCapture;
    if (camera)
    delete camera;

    if (QCamera::availableDevices().isEmpty())
        camera = new QCamera;
    else
        camera = new QCamera(QCamera::availableDevices().at(numDevice));//cameraDevice);
    cameraEnabled=true;
    //TODO: relier le cameraEnabled au systeme d'erreur de la camera
    connect(camera, SIGNAL(error(QCamera::Error)), this, SLOT(displayCameraError()));
    //debug purpose
//    for(int idxDevice=0;idxDevice<QCamera::availableDevices().count();idxDevice++)
    imageCapture = new QCameraImageCapture(camera);

    camera->setViewfinder(viewfinder);

    camera->setCaptureMode(QCamera::CaptureStillImage);
    camera->start();
    camera->searchAndLock();
}

void MainWindow::enableCamera(int state)
{
    if(state==Qt::Checked){
        if (QCamera::availableDevices().isEmpty())
        {
            disconnect(ui->cB_camera,SIGNAL(stateChanged(int)),this,SLOT(enableCamera(int)));
            ui->cB_camera->setChecked(false);
            connect(ui->cB_camera,SIGNAL(stateChanged(int)),this,SLOT(enableCamera(int)));
        }
        else
        {
            ui->comboBox_camera->setDisabled(false);
            ui->comboBox_camera->clear();
            for(int idxDevice=0;idxDevice<QCamera::availableDevices().count();idxDevice++)
            {
                ui->comboBox_camera->addItem(QCamera::deviceDescription(QCamera::availableDevices().at(idxDevice)),QVariant(idxDevice));
            }
            setCamera(ui->comboBox_camera->currentData(Qt::UserRole).toInt());
        }
    }
    if(state==Qt::Unchecked)
    {
        if (imageCapture)
            delete imageCapture;
        if (camera)
            delete camera;
        cameraEnabled=false;
        ui->comboBox_camera->setDisabled(false);
        ui->comboBox_camera->clear();
    }
}

void MainWindow::displayCameraError()
{
    QMessageBox::warning(this, tr("Camera error"), camera->errorString());
}

void MainWindow::enableAutosave(int state)
{
    if(state==Qt::Checked){
        autoSaveEnable=true;
    }
    if(state==Qt::Unchecked)
    {
        autoSaveEnable=false;
    }
}

void MainWindow::setSyncTimeAutoSave(int value)
{
    syncTimeAutoSave=value;
}

void MainWindow::updateLive(int totalRanked)
{
    int denom=countListingRacers;
    qreal Nbfinishers=(((qreal)totalRanked)/((qreal)denom))*100.0f;

    ui->progressBar_01->setValue(qFloor(Nbfinishers));
}

int MainWindow::countRacers(QTableWidget* table, QString str_subRaceChosen)
{
    int numberOfRacers=0;
    int selCol=0;
    for(int i=0;i<table->columnCount();i++)
        if(table->horizontalHeaderItem(i)->text()==str_subRace)
            selCol=i;

    for(int j=0;j<table->rowCount();j++)
        if(table->item(j,selCol)->text()==str_subRaceChosen)
            numberOfRacers++;

    return numberOfRacers;
}

/*!
 * \brief MainWindow::initRaceCharacteristics
 * \param table
 * \return
 */
bool MainWindow::initRaceCharacteristics(QTableWidget* table)
{
    int selRow=0;
    int i=0;

    QStringList labelSubRaces;
    int selCol=hdrListing[str_subRace];

    //take all the strings in subraces' name column
    for(selRow=0;selRow<table->rowCount();selRow++)
    {
        if(!(table->item(selRow,selCol)->text().isEmpty()))
        {
            labelSubRaces << table->item(selRow,selCol)->text();
        }
    }
    labelSubRaces.removeDuplicates();

    Dialog * chooseRaces=new Dialog(&labelSubRaces);
    int modifiedSubracesList=chooseRaces->exec();

    if(modifiedSubracesList==QDialog::Accepted)
    {
        subRaces.clear();
        if(labelSubRaces.isEmpty())
            subRaces<<raceName;
        else
            //store the definitiv subraces names
            subRaces=labelSubRaces;
    }
    else
    {
        subRaces.clear();
        subRaces<<raceName;
    }


    //build the tablewidget for each subrace
    if (subRaces.count()>1)
        for(i=1;i<subRaces.count();i++)
            ui->tabResults->insertTab(i,new QWidget(),subRaces.at(i));
    for(i=0;i<subRaces.count();i++)
    {
        ui->tabResults->setTabText(i,subRaces.at(i));
        countRacersList.append(0);
    }


    QGridLayout *Glayout = new QGridLayout;
    QString text("<b>Coureurs arrivés par course:<\b>");
    Glayout->addWidget(new QLabel(text),0,0,2,10);

    for(i=0;i<subRaces.count();i++)
    {
        int initWholeRacerList=0;
        //take all the strings in subraces' name column
        for(selRow=0;selRow<table->rowCount();selRow++)
        {
            if(!(table->item(selRow,selCol)->text()==subRaces.at(i)))
            {
                initWholeRacerList++;
            }
        }
        countWholeRacersList.append(initWholeRacerList);
        //qDebug()<<subRaces.at(i) << ":" <<initWholeRacerList<<" coureurs";
        //no finisher for the moment
        countFinishersList.append(0);


        //begin building header
        QStringList newHdrResults;
        newHdrResults= (QStringList()<< str_bib<<str_name<<str_frstname<<str_assoc<<str_scratch<<str_order);

        //append a column to store at least one result time
        //if we add later another chekpoints, this column will be the finisher time
        QStringList labelChkPtsInit;
        labelChkPtsInit<<str_time;
        labelChkPts.append(labelChkPtsInit);
        numberOfChkPts[subRaces.at(i)]=1;
        newHdrResults << str_time;

        //append to the list of the header titles of the tablewigdet
        hdrResults.append(newHdrResults);

        //initialisation of tablewidgets which store (and manage) results
        QTableWidget* newTableResult = new QTableWidget();
        newTableResult->setColumnCount(newHdrResults.count());
        newTableResult->setHorizontalHeaderLabels(newHdrResults);
        QGridLayout *layout = new QGridLayout;
        layout->addWidget(newTableResult,0,0);
        ui->tabResults->widget(i)->setLayout(layout);

        //add the new results tablewidget to the list
        subRacesResults.append(newTableResult);

        QProgressBar* newProgressBar= new QProgressBar();

        Glayout->addWidget(new QLabel(subRaces.at(i)),i+1,0,1,2);
        Glayout->addWidget(newProgressBar,i+1,2,1,8);

        subRacesProgress.append(newProgressBar);
    }

    ui->frameRacersProgress->setLayout(Glayout);


    //link callbacks for cells of tablewidget modifications
    for(i=0;i<subRaces.count();i++)
        connect(subRacesResults.at(i),SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));


    //for the moment no problem in init
    return true;
}

 /**
  * @brief MainWindow::addCheckPoint
  */
 void MainWindow::addCheckPoint(void)
 {
     //for the current subrace chosen (checked results list)
     int idxSubRace=ui->tabResults->currentIndex();
     //desactivate callback which look for a cell modification
     disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
     int idxToAdd=hdrResults.at(idxSubRace).count()-1;
     subRacesResults[idxSubRace]->insertColumn(idxToAdd);
     QString stringHeader;
     if(ui->comboBox_ChkPoints->currentText()=="--")
     {
         stringHeader=QString("CP_%1").arg(numberOfChkPts[subRaces.at(idxSubRace)]);
     }
     else
         stringHeader=ui->comboBox_ChkPoints->currentText();
        hdrResults[idxSubRace].insert(idxToAdd,stringHeader);
     subRacesResults[idxSubRace]->setHorizontalHeaderLabels(hdrResults[idxSubRace]);
     int idxChkPts=labelChkPts[idxSubRace].count()-1;
     labelChkPts[idxSubRace].insert(idxChkPts,stringHeader);
     int nbChkPts=numberOfChkPts[subRaces.at(idxSubRace)]+1;
     numberOfChkPts[subRaces.at(idxSubRace)]=nbChkPts;

     ui->comboBox_ChkPoints->setCurrentIndex(0);
     connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));

 }
 void MainWindow::newCheckPoint(int idxSubRace, QString checkPointName)
 {
     int idxToAdd=hdrResults.at(idxSubRace).count()-1;
     subRacesResults[idxSubRace]->insertColumn(idxToAdd);
     hdrResults[idxSubRace].insert(idxToAdd,checkPointName);
     subRacesResults[idxSubRace]->setHorizontalHeaderLabels(hdrResults[idxSubRace]);
     int idxChkPts=labelChkPts[idxSubRace].count()-1;
     labelChkPts[idxSubRace].insert(idxChkPts,checkPointName);

     //qDebug() << "insertion "<<checkPointName<< "dans" << subRaces.at(idxSubRace);
 }

 /**
  * @brief MainWindow::addCheckPoint
  */
 void MainWindow::deleteCheckPoint(void)
 {
     //for the current subrace chosen (checked results list)
     int idxSubRace=ui->tabResults->currentIndex();
     if(numberOfChkPts[subRaces.at(idxSubRace)]>1)
     {
         //desactivate callback which look for a cell modification
         disconnect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
         int idxToDel=hdrResults.at(idxSubRace).count()-2;
         subRacesResults[idxSubRace]->removeColumn(idxToDel);
         hdrResults[idxSubRace].removeAt(idxToDel);

         //subRacesResults[idxSubRace]->setHorizontalHeaderLabels(hdrResults[idxSubRace]);
         int idxChkPts=labelChkPts[idxSubRace].count()-2;
         labelChkPts[idxSubRace].removeAt(idxChkPts);
         int nbChkPts=numberOfChkPts[subRaces.at(idxSubRace)]-1;
         numberOfChkPts[subRaces.at(idxSubRace)]=nbChkPts;
         connect(subRacesResults[idxSubRace],SIGNAL(cellChanged(int,int)),this,SLOT(modifyResults(int,int)));
     }

 }

 Dialog::Dialog(QStringList* a_oResult) : m_oResult(a_oResult)
 {
     int i;

     QGridLayout *layout = new QGridLayout;

     QString text("<b>Veuillez choisir une ou plusieurs courses: <\b>");
     layout->addWidget(new QLabel(text),0,0,1,2);

      for(i=0;i<m_oResult->count();i++)
      {
          QCheckBox * chckBx=new QCheckBox(m_oResult->at(i));
          chckBx->setChecked(true);
          layout->addWidget(chckBx,i+1,0,1,2);
          chkBoxList.append(chckBx);
          connect(chckBx, SIGNAL(clicked()),this,SLOT(changeList()));
      }

      QPushButton * okButton=new QPushButton("OK");
      okButton->setDefault(true);
      connect(okButton,SIGNAL(clicked()),this, SLOT(accept()));
      //QPushButton * clcButton=new QPushButton("Annuler");
      //connect(clcButton,SIGNAL(clicked()),this, SLOT(reject()));

      layout->addWidget(okButton,m_oResult->count()+1,0,1,2);
      //layout->addWidget(clcButton,m_oResult->count()+1,1);

      setLayout(layout);

 }

 void Dialog::changeList(void)
 {
     int idxSubRace=0;
     //look for the pointer of the sender object in our table
     //to not cast the object later
     QObject* objSender = sender();
     if( objSender != NULL )
     {
         //and we determine the index of the subrace
         for(int i=0;i<chkBoxList.count();i++)
         if(objSender==chkBoxList[i])
             idxSubRace=i;
     }

     //QCheckBox * checkBoxSender=qobject_cast<QCheckBox *>(objSender);

     QString s_subRace=chkBoxList[idxSubRace]->text();

     if(chkBoxList[idxSubRace]->isChecked())
         m_oResult->append(s_subRace);
     else
     {
         if(m_oResult->count()<=1)
             chkBoxList[idxSubRace]->setChecked(true);
         else
         {
             int idx=m_oResult->indexOf(s_subRace);
             m_oResult->removeAt(idx);
         }
     }


     qDebug()<<*m_oResult;

 }

 /*!
  * \brief MainWindow::getTime give formatted string as the time since the start of the race
  * \return
  */
 QString MainWindow::getTime(const char c_format)
 {
     QDateTime now=QDateTime::currentDateTime();

     //extract hour, minute and seconde from the current time
     qreal hour=(qreal)initStart.secsTo(now)/3600.0f;
     qreal minute=(hour-(qreal)qFloor(hour))*60.0f;
     qreal seconde=(minute-(qreal)qFloor(minute))*60.0f;

     //format the string like "hh:mm:ss" by default
     QString returnTime=QString("%1%2%3%4%5").arg(qFloor(hour),2,10,QLatin1Char('0'))
             .arg(c_format)
             .arg(qFloor(minute),2,10,QLatin1Char('0'))
             .arg(c_format)
             .arg(qFloor(seconde),2,10,QLatin1Char('0'));

     return returnTime;
 }
