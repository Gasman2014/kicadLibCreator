#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QFileInfo>
#include <QJsonDocument>
#include <QDirIterator>
#include <QUuid>
#include <QPalette>
#include "kicadfile_lib.h"
#include "ruleeditor.h"
#include "optionsdialog.h"
#include <QDesktopServices>
#include <QCompleter>
#include <assert.h>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    octopartInterface("",parent)

{
    ui->setupUi(this);
    connect(ui->comboBox->lineEdit(), SIGNAL(returnPressed()),
            this, SLOT(oncmbOctoQueryEnterPressed()));

    connect(&octopartInterface, SIGNAL(octopart_request_finished()),
            this, SLOT(octopart_request_finished()));

    connect(&octopartInterface, SIGNAL(octopart_request_started()),
            this, SLOT(octopart_request_started()));

    connect(&octopartInterface, SIGNAL(setProgressbar(int,int)),
            this, SLOT(setProgressbar(int, int)));

    libCreatorSettings.loadSettings("kicadLibCreatorSettings.ini");
    fpLib.scan(libCreatorSettings.path_footprintLibrary);

    octopartInterface.setAPIKey(libCreatorSettings.apikey);
    partCreationRuleList.loadFromFile("partCreationRules.ini");
    querymemory.loadQueryList(ui->comboBox);
    libCreatorSettings.complainAboutSettings(this);
    progressbar = new QProgressBar(ui->statusBar);
    progressbar->setVisible(false);
    ui->statusBar->addPermanentWidget(progressbar);
    selectedOctopartMPN.setDebugPrintMpn(true);
    // ui->lblSpinner->setVisible(false);
    ui->cbtn_exact_match->setChecked(!libCreatorSettings.useFuzzyOctopartQueries);
}

void MainWindow::setProgressbar(int progress, int total)
{

    progressbar->setValue(progress);
    progressbar->setMaximum(total);
}

void MainWindow::octopart_request_started(){
    progressbar->setValue(0);
    progressbar->setMaximum(100);
    progressbar->setVisible(true);
    ui->statusBar->showMessage("Sending request..", 500);
}

void MainWindow::octopart_request_finished()
{
    progressbar->setVisible(false);
  //  qDebug() << "httpFinished";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent (QCloseEvent *event){
    octopartCategorieCache.save();
    libCreatorSettings.saveSettings();
    querymemory.save();
    partCreationRuleList.saveFile("partCreationRules.ini");
    event->accept();
}


void MainWindow::on_pushButton_clicked() {


    resetSearchQuery(true);
    octopartInterface.sendMPNQuery(octopartCategorieCache, ui->comboBox->currentText(),!ui->cbtn_exact_match->isChecked());
    queryResults.clear();
    queryResults.append(octopartInterface.octopartResult_QueryMPN);

    ui->tableOctopartResult->setRowCount(queryResults.length());

    for(int i=0;i<queryResults.length();i++){
        QTableWidgetItem *newMPNItem = new QTableWidgetItem(queryResults[i].getMpn());
        QTableWidgetItem *newManufacturerItem = new QTableWidgetItem(queryResults[i].manufacturer);
        QTableWidgetItem *newFootprint = new QTableWidgetItem(queryResults[i].footprint);
        QTableWidgetItem *newCategorie = new QTableWidgetItem("");
        QTableWidgetItem *newDescription = new QTableWidgetItem(queryResults[i].description);
        QTableWidgetItem *newExtras = new QTableWidgetItem("");
        QTableWidgetItem *newOctopartURL = new QTableWidgetItem("link");
        newOctopartURL->setData(Qt::UserRole,queryResults[i].urlOctoPart);
        newMPNItem->setData(Qt::UserRole,i);
        QString categorie_str;
        for(int n=0;n<queryResults[i].categories.count();n++){
            for(int m=0;m<queryResults[i].categories[n].categorieNameTree.count();m++){
                categorie_str += queryResults[i].categories[n].categorieNameTree[m] +", ";
            }
            categorie_str += "\n";
        }
        QString ds;
        if (queryResults[i].urlDataSheet == ""){
            ds = "Datasheets: no\n";
        }else{
            ds = "Datasheets: yes\n";
        }
        if (queryResults[i].url3DModel == ""){
            ds += "3DModel: no";
        }else{
            ds += "3DModel: yes";
        }
        newExtras->setText(ds);
        newCategorie->setText(categorie_str);
        ui->tableOctopartResult->setItem(i,0,newMPNItem);
        ui->tableOctopartResult->setItem(i,1,newManufacturerItem);
        ui->tableOctopartResult->setItem(i,2,newDescription);
        ui->tableOctopartResult->setItem(i,3,newFootprint);
        ui->tableOctopartResult->setItem(i,4,newCategorie);
        ui->tableOctopartResult->setItem(i,5,newExtras);
        ui->tableOctopartResult->setItem(i,6,newOctopartURL);

    }
    ui->tableOctopartResult->resizeColumnsToContents();
    ui->tableOctopartResult->resizeRowsToContents();
    //octopartInterface.sendOctoPartRequest("SN74S74N");

}



void MainWindow::on_tableOctopartResult_cellDoubleClicked(int row, int column)
{
    if (column == 6){
        if (row < queryResults.count()){
            ui->statusBar->showMessage("Open Browser..", 2000);
            QDesktopServices::openUrl(QUrl(queryResults[row].urlOctoPart));
        }
    }else{
        ui->tabWidget->setCurrentIndex(1);
    }
}

void MainWindow::on_tableOctopartResult_cellClicked(int row, int column)
{
    if (column == 6){
        if (row < queryResults.count()){
            ui->statusBar->showMessage("Open Browser..", 2000);
            QDesktopServices::openUrl(QUrl(queryResults[row].urlOctoPart));
        }
    }
}


void MainWindow::on_tableOctopartResult_cellActivated(int row, int column)
{

    (void)column;
    (void)row;
}

void MainWindow::on_tableOctopartResult_itemSelectionChanged()
{
    resetSearchQuery(false);
    int row = ui->tableOctopartResult->currentRow();
   // qDebug() << "row selected:"<<row<<"queryResults.count():" << queryResults.count();
    if (row < queryResults.count()){
        selectedOctopartMPN.copyFrom(queryResults[row]);
    }
}

void MainWindow::on_comboBox_editTextChanged(const QString &arg1)
{
    (void)arg1;
    if (!ui->comboBox->property("ignoreChanges").toBool()){
        resetSearchQuery(true);
    }
}

void MainWindow::on_comboBox_currentTextChanged(const QString &arg1)
{
    (void)arg1;
    //on_comboBox_editTextChanged(arg1);
}

void MainWindow::resetSearchQuery(bool resetAlsoTable)
{
    if (resetAlsoTable){
        ui->tableOctopartResult->clear();
        ui->tableOctopartResult->setRowCount(0);
        ui->tableOctopartResult->setColumnCount(7);
        QStringList hHeader;
        hHeader << "MPN" << "Manufacturer" << "Description" << "Footprint" << "Categories" << "Extras" << "Octopart";
        ui->tableOctopartResult->setHorizontalHeaderLabels(hHeader);
        queryResults.clear();
    }
    selectedOctopartMPN.clear();
    currentSourceDevice.clear();

    ui->cmb_targetFootprint->setCurrentIndex(-1);
    ui->cmb_targetLibrary->setCurrentIndex(-1);
    ui->cmb_targetRuleName->setCurrentIndex(-1);
    ui->edt_targetDatasheet->setText("");
    ui->edt_targetDescription->setText("");
    ui->edt_targetDesignator->setText("");
    ui->edt_targetDisplayValue->setText("");
    ui->edt_targetID->setText("");
    ui->edt_targetManufacturer->setText("");
    ui->edt_targetMPN->setText("");
    ui->edt_targetName->setText("");

}

void MainWindow::clearQuickLinks(QLayout *layout)
{
    while (QLayoutItem* item = layout->takeAt(0))
    {
        if (QWidget* widget = item->widget())
            delete widget;
        if (QLayout* childLayout = item->layout())
            clearQuickLinks(childLayout);
        delete item;
    }
}



void MainWindow::on_tabWidget_currentChanged(int index)
{
     if (index == 0){
         querymemory.loadQueryList(ui->comboBox);
    }else if (index == 1){
        if (ui->tableOctopartResult->currentRow() == -1){
            if (queryResults.count() == 1){
                //qDebug() << "proceed because just one line found.";
                selectedOctopartMPN.copyFrom(queryResults[0]);
            }
        }
#if 0
        qDebug() << "query result count: "<<queryResults.count();
        for(int i=0;i< queryResults.count();i++){
            qDebug() << "query result["<<i<<"] ="<< queryResults[i].toString();
        }
        qDebug() << "current query result: "<<selectedOctopartMPN.toString();
#endif
        if (selectedOctopartMPN.getMpn() == ""){
            ui->tabWidget->setCurrentIndex(0);
            ui->statusBar->showMessage("Please select a part from octopart query first 1", 2000);
        }else{
            ui->lbl_sourceOctopart->setText("Selected Octopart MPN: "+selectedOctopartMPN.getMpn());
            querymemory.addQuery(selectedOctopartMPN.getMpn());
            sourceLibraryPaths.clear();
            QDirIterator it(libCreatorSettings.path_sourceLibrary, QDirIterator::NoIteratorFlags);
            while (it.hasNext()) {
                QString name = it.next();
                QFileInfo fi(name);
                if (fi.suffix()=="lib"){
                    sourceLibraryPaths.append(name);
                }
            }
            //ui->cmb_targetRuleName->setCurrentText("");
            ui->list_input_libraries->clear();
            sourceLibraryPaths.sort(Qt::CaseInsensitive);
            for(int i=0;i<sourceLibraryPaths.count();i++){
                QFileInfo fi(sourceLibraryPaths[i]);
                ui->list_input_libraries->addItem(fi.fileName());
            }
            ui->scrollQuickLink->setWidgetResizable(true);


            clearQuickLinks(ui->verticalLayout_3);

            QList<PartCreationRule> possibleRules = partCreationRuleList.findRuleByCategoryID(selectedOctopartMPN.categories);

            for (auto possibleRule : possibleRules){
                QLabel *lbl = new QLabel(ui->scrollAreaWidgetContents);
                auto sl = possibleRule.links_source_device;
                for (auto s : sl){
                    QString ruleName = possibleRule.name;
                    lbl->setText("<a href=\""+s+"/"+ruleName+"\">"+s+" ("+ruleName+")</a>");
                    lbl->setTextFormat(Qt::RichText);
                    ui->verticalLayout_3->addWidget(lbl);
                    connect(lbl,SIGNAL(linkActivated(QString)),this,SLOT(onQuickLinkClicked(QString)));
                }
            }
        }

    }else if(index == 2){
        if (currentSourceDevice.def.name.count() == 0){
            ui->tabWidget->setCurrentIndex(1);
            ui->statusBar->showMessage("Please select a source device from Kicad library first", 2000);
        }else{
            if (selectedOctopartMPN.getMpn() == ""){
                ui->tabWidget->setCurrentIndex(0);
                ui->statusBar->showMessage("Please select a part from octopart query first", 2000);
            }else{
                querymemory.addQuery(selectedOctopartMPN.getMpn());
                ui->cmb_targetFootprint->clear();
                ui->cmb_targetFootprint->addItems(fpLib.getFootprintList());
                ui->lblSourceDevice->setText(currentSourceLib.getName()+"/"+currentSourceDevice.def.name);

                QStringList targetLibList;
                ui->cmb_targetLibrary->clear();
                QDirIterator it(libCreatorSettings.path_targetLibrary, QDirIterator::NoIteratorFlags);
                while (it.hasNext()) {
                    QString name = it.next();
                    QFileInfo fi(name);
                    if (fi.suffix()=="lib"){
                        targetLibList.append(name);
                    }
                }
                targetLibList.sort(Qt::CaseInsensitive);
                for(int i=0;i<targetLibList.count();i++){
                    QFileInfo fi(targetLibList[i]);
                    ui->cmb_targetLibrary->addItem(fi.fileName());
                }

                loadRuleCombobox();
                on_btn_applyRule_clicked();
            }
        }
    }
}

void  MainWindow::loadRuleCombobox(){
    QString selectedRule = ui->cmb_targetRuleName->currentText();
    ui->cmb_targetRuleName->clear();
    ui->cmb_targetRuleName->addItems(partCreationRuleList.namesWithoutGlobal);
    ui->cmb_targetRuleName->setCurrentText(selectedRule);


}



void MainWindow::onQuickLinkClicked(QString s)
{
    bool found = true;
    auto sl = s.split("/");
    assert(sl.count() > 0);
    auto devLibFinds = ui->list_input_libraries->findItems(sl[0],Qt::MatchExactly);
    if (devLibFinds.count()){
        auto devLib = devLibFinds[0];
        ui->list_input_libraries->setCurrentItem(devLib);
    }else{
        found = false;
    }

    if (found){
        assert(sl.count() > 1);
        QString partname = sl[1];
        for (int i = 2; i< sl.count()-1; i++){
            partname += "/"+sl[i];
        }
        auto devFinds = ui->list_input_devices->findItems(partname,Qt::MatchExactly);
        if (devFinds.count()){
            auto dev = devFinds[0];
            ui->list_input_devices->setCurrentItem(dev);
            on_list_input_devices_currentRowChanged(ui->list_input_devices->currentRow());
        }else{
            found = false;
        }
    }
    if (found){
        assert(sl.count() > 2);
        int rulePosition = sl.count()-1;
        ui->cmb_targetRuleName->setCurrentText(sl[rulePosition]);
        ui->tabWidget->setCurrentIndex(2);
    }
}





void MainWindow::on_list_input_libraries_currentRowChanged(int currentRow)
{
   // qDebug() << "row" << currentRow;
   // qDebug() << "rowcount" << sourceLibraryPaths.count();

    if ((currentRow > -1) && currentRow < sourceLibraryPaths.count()){
        currentSourceLib.loadFile(sourceLibraryPaths[currentRow]);

        QList<KICADLibSchematicDevice> symList = currentSourceLib.getSymbolList();
        ui->list_input_devices->clear();
        ui->list_input_devices->setCurrentRow(0);
        for(int i=0;i<symList.count();i++){
            ui->list_input_devices->addItem(symList[i].def.name);
        }
    }
}


void MainWindow::on_list_input_devices_currentRowChanged(int currentRow)
{
    QList<KICADLibSchematicDevice> symList = currentSourceLib.getSymbolList();
    if ((currentRow < symList.count()) && (currentRow >= 0)){
        currentSourceDevice = symList[currentRow];
    }else{
        currentSourceDevice.clear();
    }
}

void MainWindow::on_list_input_devices_itemDoubleClicked(QListWidgetItem *item)
{
    (void)item;
    ui->tabWidget->setCurrentIndex(2);
}

void MainWindow::setCurrentDevicePropertiesFromGui()
{
    KICADLibSchematicDeviceField deviceField;

    deviceField.clear();
    deviceField.name = "key";
    deviceField.text = ui->edt_targetID->text();//QUuid::createUuid().toByteArray().toHex();
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(4);
    currentSourceDevice.fields.setField(deviceField);

    deviceField.clear();
    deviceField.name = "footprint";
    deviceField.text = ui->cmb_targetFootprint->currentText();
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(2);
    currentSourceDevice.fields.setField(deviceField);

    deviceField.clear();
    deviceField.name = "datasheet";

    deviceField.text = getDataSheetFileName(true);
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(3);
    currentSourceDevice.fields.setField(deviceField);

    deviceField.clear();
    deviceField.name = "mpn";
    deviceField.text = ui->edt_targetMPN->text();
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(5);
    currentSourceDevice.fields.setField(deviceField);

    deviceField.clear();
    deviceField.name = "manufacturer";
    deviceField.text = ui->edt_targetManufacturer->text();
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(6);
    currentSourceDevice.fields.setField(deviceField);


    deviceField.clear();
    deviceField = currentSourceDevice.fields.getFieldbyIndex(0);
    deviceField.text = ui->edt_targetDesignator->text();
    deviceField.name = "";
    deviceField.fieldIndex.setRawIndex(0);
    currentSourceDevice.fields.setField(deviceField);


    deviceField.clear();
    deviceField = currentSourceDevice.fields.getFieldbyIndex(1); //lets copy value field and replace it optically by disp value
    deviceField.name = "DisplayValue";
    deviceField.text = ui->edt_targetDisplayValue->text();
    deviceField.visible = true;
    deviceField.fieldIndex.setRawIndex(7);
    currentSourceDevice.fields.setField(deviceField);


    deviceField.clear();
    deviceField = currentSourceDevice.fields.getFieldbyIndex(1);
    deviceField.name = "";
    deviceField.text = ui->edt_targetMPN->text();
    deviceField.visible = false;
    deviceField.fieldIndex.setRawIndex(1);
    currentSourceDevice.fields.setField(deviceField);

    currentSourceDevice.fields.removeAllAboveIndex(8);


    currentSourceDevice.fpList.clear();//we want our own footprints..

    currentSourceDevice.dcmEntry.description = ui->edt_targetDescription->text();
    currentSourceDevice.def.reference =  ui->edt_targetDesignator->text();
    currentSourceDevice.def.name = ui->edt_targetMPN->text();
}

QString MainWindow::cleanUpFileNameNode(QString filename,bool allowSeparatorLikeChars){
    filename = filename.replace("=","_");
    filename = filename.replace("[","_");
    filename = filename.replace("]","_");
    filename = filename.replace("\"","_");
    //dont want to destroy pathes like C:\...
    QChar second_char;
    if (filename.count() > 1){
        second_char = filename[1];
    }else{
        second_char = '0';
    }
    filename = filename.replace(":","_");
    if ((filename.count() > 1) && (second_char != '0')){
        filename[1] = second_char;
    }
    filename = filename.replace(";","_");
    filename = filename.replace(",","_");
    filename = filename.replace("?","_");
    filename = filename.replace("*","_");
    filename = filename.replace("<","_");
    filename = filename.replace(">","_");
    filename = filename.replace("|","_");
    filename = filename.replace("&","_");
    filename = filename.replace(" ","_");
    if(!allowSeparatorLikeChars){
        filename = filename.replace("/","_");
        filename = filename.replace("\\","_");
    }
    filename = filename.toLower();
    return filename;
}

QString MainWindow::cleanUpFileName(QString filename)
{
    QString result;
    result = cleanUpFileNameNode(filename,true);
    result = result.replace(QString(QDir::separator())+QDir::separator(),QDir::separator());
    return result;
}



QString MainWindow::getDataSheetFileName(bool relativePath){

    QString targetpath=libCreatorSettings.path_datasheet+QDir::separator();
    if (relativePath){
        targetpath = ui->edt_targetDatasheet->text();
    }else{
        targetpath=libCreatorSettings.path_datasheet+QDir::separator()+ui->edt_targetDatasheet->text();
    }

    targetpath = cleanUpFileName(targetpath);
    return targetpath;
}

void MainWindow::downloadDatasheet(bool force){
    RESTRequest restRequester;
    QBuffer buf;
    QMultiMap<QString, QString> params;

    connect(&restRequester, SIGNAL(http_request_finished()),
            this, SLOT(octopart_request_finished()));

    connect(&restRequester, SIGNAL(http_request_started()),
            this, SLOT(octopart_request_started()));

    connect(&restRequester, SIGNAL(setProgressbar(int,int)),
            this, SLOT(setProgressbar(int, int)));

    QString targetpath = getDataSheetFileName(false);
    QFileInfo fi(targetpath);
    bool proceed = true;
    if (!fi.isAbsolute()){
        if (QMessageBox::warning(this,"path is relative","Datasheet target path \n\n"+targetpath+"\n\nis not an absolute path. The datasheet will be downloaded relative to the execution path of this application.\n\n"
                                                                                 "You should change this in the settings. Do you want to proceed?",QMessageBox::Yes | QMessageBox::No, QMessageBox::No)==QMessageBox::No){
            proceed = false;
        }

    }
    QString p = fi.absolutePath();
    if ((force || !fi.exists()) && proceed ){
        ui->pushButton_3->setText("downloading..");
        QDir().mkpath(p);
        restRequester.startRequest(selectedOctopartMPN.urlDataSheet,params,&buf);
        QFile file(targetpath);
        ui->pushButton_3->setText("download");

        if (file.open(QIODevice::WriteOnly)){
            buf.open(QIODevice::ReadOnly);
            file.write(buf.readAll());
            file.close();
            ui->statusBar->showMessage("saved datasheet to "+targetpath,5000);
        }else{
            ui->statusBar->showMessage("couldnt open file "+targetpath,5000);
            qDebug() << "couldnt open file "+targetpath;
        }
    }
    setDatasheetButton();
}



void MainWindow::on_pushButton_3_clicked()
{
downloadDatasheet(true);
}

void MainWindow::on_btnCreatePart_clicked()
{
    KICADLibSchematicDeviceLibrary targetLib;
    setCurrentDevicePropertiesFromGui();
    QFileInfo fi(ui->cmb_targetLibrary->currentText());
    QString targetLibName;
    if (fi.suffix() == "lib"){
        targetLibName= libCreatorSettings.path_targetLibrary+QDir::separator()+ui->cmb_targetLibrary->currentText();
    }else{
        targetLibName= libCreatorSettings.path_targetLibrary+QDir::separator()+ui->cmb_targetLibrary->currentText()+".lib";
    }
    targetLibName = cleanUpFileName(targetLibName);
    downloadDatasheet(false);
    if (currentSourceDevice.isValid()){
        targetLib.loadFile(targetLibName);
        bool proceed = true;
        if (targetLib.indexOf(currentSourceDevice.def.name)>-1){
            QMessageBox msgBox;
            msgBox.setText("Part duplicate");
            msgBox.setInformativeText("A part with this name already exists. Overwrite old part?");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            proceed =  QMessageBox::Ok == msgBox.exec();
        }
        if(proceed){

            targetLib.insertDevice(currentSourceDevice);
            if (targetLib.saveFile(targetLibName)){
                ui->statusBar->showMessage("Inserted part "+currentSourceDevice.def.name+" to library: \""+targetLibName+"\"",10000);
            }else{
                ui->statusBar->showMessage("Could not save library \""+targetLibName+"\"",10000);
            }
        }
    }else{
        QMessageBox::warning(this, "current device invalid", "current device invalid");
    }
}



void MainWindow::on_lbl_targetFootprint_linkActivated(const QString &link)
{
    ui->cmb_targetFootprint->setCurrentText(link);
}


void MainWindow::on_cmb_targetFootprint_currentTextChanged(const QString &arg1)
{
    if (ui->cmb_targetFootprint->findText(arg1) == -1){
        QPalette pal = ui->edt_targetDescription->palette();
        pal.setColor(ui->cmb_targetFootprint->backgroundRole(), Qt::red);
        pal.setColor(ui->cmb_targetFootprint->foregroundRole(), Qt::red);
        ui->cmb_targetFootprint->setPalette(pal);
    }else{
        QPalette pal = ui->edt_targetDescription->palette();
        ui->cmb_targetFootprint->setPalette(pal);
    }
}


void MainWindow::insertStandardVariablesToMap(QMap<QString, QString> &variables, QString footprint, QString reference, QString ruleName, QString mpn,
                                              QString manufacturer, QString description, QString OctoFootprint ){
    if (footprint.count()){
        variables.insert("%source.footprint%",footprint);
    }
    if (reference.count()){
        variables.insert("%source.ref%",reference);
    }
    if (ruleName.count()){
        variables.insert("%rule.name%",ruleName);
        variables.insert("%rule.name.saveFilename%",cleanUpFileNameNode(ruleName,false));
    }
    variables.insert("%octo.mpn%",mpn);
    variables.insert("%octo.manufacturer%",manufacturer);
    variables.insert("%octo.description%",description);
    variables.insert("%octo.footprint%",OctoFootprint);

    variables.insert("%octo.mpn.saveFilename%",cleanUpFileNameNode(mpn,false));
    variables.insert("%octo.manufacturer.saveFilename%",cleanUpFileNameNode(manufacturer,false));


    variables.insert("%target.id%",QString::number(QDateTime::currentMSecsSinceEpoch()));
}

QMap<QString, QString> MainWindow::createVariableMap(){
    QMap<QString, QString> variables = selectedOctopartMPN.getQueryResultMap();
    QString fp = currentSourceDevice.fields.getFieldbyIndex(3).text;
    QString footprint;
    QString reference = currentSourceDevice.def.reference;
    QString ruleName = ui->cmb_targetRuleName->currentText();
    QString mpn = selectedOctopartMPN.getMpn();
    QString manufacturer = selectedOctopartMPN.manufacturer;
    QString description = selectedOctopartMPN.description;
    QString OctoFootprint = selectedOctopartMPN.footprint;
    if (fp.count()){
       footprint = currentSourceDevice.fields.getFieldbyIndex(3).text;
    }

    insertStandardVariablesToMap(variables, footprint,reference,ruleName, mpn, manufacturer,description, OctoFootprint );


    return variables;
}

void MainWindow::on_actionEdit_triggered()
{
    RuleEditor ruleeditor(this);
    QStringList proposedCategories;
    QStringList proposedSourceDevices;
    ruleeditor.setRules(partCreationRuleList,  proposedCategories,  proposedSourceDevices);
    ruleeditor.exec();
    if(ruleeditor.result()){
        partCreationRuleList = ruleeditor.getRules();
        partCreationRuleList.modified();
        loadRuleCombobox();

    }

}

void MainWindow::on_btn_editRule_clicked()
{
    RuleEditor ruleeditor(this);
    QStringList proposedCategories;
    QStringList proposedSourceDevices;

    proposedSourceDevices << currentSourceLib.getName() + "/"+currentSourceDevice.def.name;
    for (auto cat: selectedOctopartMPN.categories){
        QString name = cat.categorie_uid+"~";
        for (auto str: cat.categorieNameTree){
            name += str+"/";
        }
        proposedCategories.append(name) ;
    }


    ruleeditor.setRules(partCreationRuleList,  proposedCategories,  proposedSourceDevices);
    ruleeditor.setCurrenRule(ui->cmb_targetRuleName->currentText());
    ruleeditor.setVariables(createVariableMap());
    ruleeditor.exec();
    if(ruleeditor.result()){
        partCreationRuleList = ruleeditor.getRules();
        partCreationRuleList.modified();
        loadRuleCombobox();
    }
}

void MainWindow::on_btn_applyRule_clicked()
{
    targetDevice = currentSourceDevice;
    PartCreationRule currentRule =  partCreationRuleList.getRuleByNameForAppliaction(ui->cmb_targetRuleName->currentText());

  //  if (currentRule.name != "")
    // even if name is empty a global rule may still be active.
    {

        QMap<QString, QString> variables = createVariableMap();
        PartCreationRuleResult creationRuleResult = currentRule.setKicadDeviceFieldsByRule(variables);
        ui->edt_targetDesignator->setText(creationRuleResult.designator);
        ui->edt_targetName->setText( creationRuleResult.name);
        ui->cmb_targetFootprint->setCurrentText( creationRuleResult.footprint);
        ui->lbl_targetFootprint->setText("<a href="+ creationRuleResult.footprint+">"+creationRuleResult.footprint+"</a>");
        ui->edt_targetDatasheet->setText( creationRuleResult.datasheet);
        ui->edt_targetID->setText( creationRuleResult.id);
        ui->edt_targetMPN->setText( creationRuleResult.mpn);
        ui->edt_targetManufacturer->setText( creationRuleResult.manufacturer);
        ui->edt_targetDescription->setText( creationRuleResult.description);
        ui->edt_targetDisplayValue->setText( creationRuleResult.display_value);
        ui->cmb_targetLibrary->setCurrentText( creationRuleResult.lib_name);

    }

}

void MainWindow::oncmbOctoQueryEnterPressed()
{
    on_pushButton_clicked();
}


void MainWindow::setDatasheetButton()
{
    QString targetpath = getDataSheetFileName(false);
    QFileInfo fi(targetpath);
    ui->btn_show_datasheet->setEnabled(fi.exists());
}




void MainWindow::on_edt_targetDatasheet_textChanged(const QString &arg1)
{
    (void)arg1;
    setDatasheetButton();
}

void MainWindow::on_btn_show_datasheet_clicked()
{
    QString targetpath = getDataSheetFileName(false);
    QString url = "file:///"+targetpath;
    if (QDesktopServices::openUrl(QUrl(url))==false){
        ui->statusBar->showMessage("Could not open file with default brower: "+url,5000);
    }
}

void MainWindow::on_actionOptions_triggered()
{
    OptionsDialog diag(libCreatorSettings);
    diag.exec();
    octopartInterface.setAPIKey(libCreatorSettings.apikey);
    libCreatorSettings.complainAboutSettings(this);
}











