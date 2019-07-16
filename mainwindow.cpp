#include <cmath>

#include <QButtonGroup>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeyEvent>
#include <QAction>
#include <QStandardPaths>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QFile>
#include <QFileSystemModel>

#include <QDebug>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <math.h>


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	table = ui->tableWidget;

	QButtonGroup* outputTypeGroup = new QButtonGroup();
	outputTypeGroup->addButton(ui->OUT_HEX);
	outputTypeGroup->addButton(ui->OUT_BIN);
	outputTypeGroup->addButton(ui->OUT_DEC);
	outputTypeGroup->setExclusive(true);

	QButtonGroup* outputWidthGroup = new QButtonGroup();
	outputWidthGroup->addButton(ui->OUT_8BIT);
	outputWidthGroup->addButton(ui->OUT_16BIT);
	outputWidthGroup->addButton(ui->OUT_32BIT);
	outputWidthGroup->setExclusive(true);

	tableActionsSet();
	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	tableItemsHeaderInit();

	bitSelectStatus = new QLabel("0 bit");
	bitSelectStatus->setStyleSheet("padding: 5px");
	ui->statusBar->addPermanentWidget(bitSelectStatus);

	table->installEventFilter(this);

	storageDir = new QDir(QStandardPaths::locate(QStandardPaths::DocumentsLocation, "", QStandardPaths::LocateDirectory) + "QDataViewer");
	if (!storageDir->exists()) {
		storageDir->mkpath(".");
	}

	storageListUpdate();
	ui->STORAGE_LIST->installEventFilter(this);

	connect(table, &QTableWidget::itemSelectionChanged, this, &MainWindow::onTableSelectChange);
	connect(table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onTableCellDoubleClick);
	connect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);

	connect(ui->OUT_NOSPACE, &QPushButton::clicked, this, &MainWindow::onOutputUpdateRequest);
	connect(outputTypeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputUpdateRequest);
	connect(outputWidthGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputUpdateRequest);

	connect(ui->STORAGE_SAVE, &QPushButton::clicked, this, &MainWindow::onStorageSaveRequest);
	connect(ui->STORAGE_LOAD, &QPushButton::clicked, this, &MainWindow::onStorageLoadRequest);
	connect(ui->STORAGE_LIST, &QListWidget::itemSelectionChanged, this, &MainWindow::onStorageSelectionChanged);
	connect(ui->STORAGE_LIST, &QListWidget::itemChanged, this, &MainWindow::onStorageRenameRequest);
}

void MainWindow::tableActionsSet() {
	QAction* delAction = new QAction("Delete");
	connect(delAction, &QAction::triggered, this, &MainWindow::onColumnDeleteAction);
	table->addAction(delAction);

	QAction* addLeftAction = new QAction("Add to left");
	connect(addLeftAction, &QAction::triggered, this, &MainWindow::onColumnAddToLeftAction);
	table->addAction(addLeftAction);

	QAction* addRightAction = new QAction("Add to right");
	connect(addRightAction, &QAction::triggered, this, &MainWindow::onColumnAddToRightAction);
	table->addAction(addRightAction);
}

MainWindow::~MainWindow(){
	delete ui;
}

void MainWindow::tableItemsHeaderInit() {
	disconnect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);

	for (int i = 0, j = table->columnCount() - 1; i < table->columnCount(); i++, j--) {
		table->setHorizontalHeaderItem(i, new QTableWidgetItem(QString::number(j)));

		if (table->item(0, i) == nullptr) {
			QTableWidgetItem* bitItem = new QTableWidgetItem();
			bitItem->setTextAlignment(Qt::AlignCenter);
			bitItem->setFlags(bitItem->flags() & ~Qt::ItemIsEditable);
			bitItem->setText("0");
			table->setItem(0, i, bitItem);
		}
		if (table->item(1, i) == nullptr) {
			QTableWidgetItem* valueItem = new QTableWidgetItem();
			valueItem->setTextAlignment(Qt::AlignCenter);
			valueItem->setText("0");
			table->setItem(1, i, valueItem);
		}
		if (table->item(2, i) == nullptr) {
			QTableWidgetItem* nameItem = new QTableWidgetItem();
			nameItem->setTextAlignment(Qt::AlignCenter);
			table->setItem(2, i, nameItem);
		}
	}

	connect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);

	onOutputUpdateRequest();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
	if (watched == table && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Space) {
			tableMergeSelectedColumns();
			return true;
		}
		if (keyEvent->key() == Qt::Key_Delete) {
			QTableWidgetItem* item = table->currentItem();
			if (item->row() == 2) {
				item->setText("");
			}

		}
	}
	if (watched == ui->STORAGE_LIST && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Delete) {
			QListWidgetItem* item = ui->STORAGE_LIST->currentItem();
			storageDir->remove(item->text() + ".xml");
			storageListUpdate();
		}
	}
	return false;
}

void MainWindow::onTableFieldValueUpdate(QTableWidgetItem* item) {
	if (item->row() != 1) return;

	bool conversionSuccess;
	quint64 fieldValue;
	if (item->text().contains("0x", Qt::CaseInsensitive)) {
		fieldValue = item->text().toULongLong(&conversionSuccess, 16);
	} else {
		fieldValue = item->text().toULongLong(&conversionSuccess, 10);
	}

	if (!conversionSuccess) {
		ui->statusBar->showMessage("Failed to parse input value", 3000);
		return;
	}

	int itemCol = item->column();
	QString fieldBits = QString::number(fieldValue, 2).rightJustified(table->columnSpan(1, itemCol), '0');
	for (int i = fieldBits.size() - 1; i >= 0; i--) {
		if (table->item(0, i + itemCol) != nullptr) {
			table->item(0, i + itemCol)->setText(QString(fieldBits.at(i)));
		}
	}

	onOutputUpdateRequest();
}

void MainWindow::tableFieldValueRecalculate() {
	disconnect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);

	for (int i = 0; i < table->columnCount(); i++) {
		int colSpan = table->columnSpan(1, i);
		if (colSpan > 1) {
			QString fieldBits;
			for (int j = i; j < i + colSpan; j++) {
				fieldBits.append(table->item(0, j)->text());
			}

			quint64 fieldValue = fieldBits.toULongLong(nullptr, 2);
			QString fieldText = "0x" + QString::number(fieldValue, 16).rightJustified(ceil(colSpan / 4), '0').toUpper();
			table->item(1, i)->setText(fieldText);

			i += colSpan - 1;
		} else {
			table->item(1, i)->setText(table->item(0, i)->text());
		}
	}

	connect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);
}

QStringList MainWindow::tableBitsToChunks(int size) {
	QStringList chunks;
	QString chunk;

	for (int i = 0; i < table->columnCount(); i++) {
		if (table->item(0, i) != nullptr) {
			chunk.append(table->item(0, i)->text());
		} else {
			chunk.append("0");
		}
		if (chunk.size() >= size) {
			chunks.append(chunk);
			chunk.clear();
		}
	}
	if (!chunk.isEmpty()) {
		chunks.append(chunk);
	}

	return chunks;
}

void MainWindow::onOutputUpdateRequest(int btnId) {
	//Process table data and spit it to OUT field
	QStringList inputChunks;

	if (ui->OUT_8BIT->isChecked()) {
		inputChunks = tableBitsToChunks(8);
	} else if (ui->OUT_16BIT->isChecked()) {
		inputChunks = tableBitsToChunks(16);
	} else if (ui->OUT_32BIT->isChecked()) {
		inputChunks = tableBitsToChunks(32);
	}

	QStringList chunkTexts;
	foreach (QString chunk, inputChunks) {
		uint32_t chunkValue = chunk.toULong(nullptr, 2);
		if (ui->OUT_HEX->isChecked()) {
			QString chunkText;
			if (!ui->OUT_NOSPACE->isChecked()) {
				chunkText = "0x";
			}
			chunkText += QString::number(chunkValue, 16).rightJustified(chunk.size() / 4, '0').toUpper();
			chunkTexts.append(chunkText);
		} else if (ui->OUT_DEC->isChecked()) {
			chunkTexts.append(QString::number(chunkValue));
		} else if (ui->OUT_BIN->isChecked()) {
			chunkTexts.append(chunk);
		}
	}

	ui->OUT->setText(chunkTexts.join(ui->OUT_NOSPACE->isChecked() ? "" : " "));
}

void MainWindow::onTableCellDoubleClick(int row, int column) {
	if (row != 0) return;

	if (table->item(0, column)->text() == "0") {
		table->item(0, column)->setText("1");
	} else {
		table->item(0, column)->setText("0");
	}	

	tableFieldValueRecalculate();
	onOutputUpdateRequest();
}

QList<int> MainWindow::tableSelectedColumns() {
	QItemSelectionModel* selectionModel = table->selectionModel();

	QList<int> selectedColumns;
	QModelIndexList indexList = selectionModel->selection().indexes();
	foreach (const QModelIndex &index , indexList) {
		if (!selectedColumns.contains(index.column())) {
			selectedColumns.append(index.column());
		}
	}

	return selectedColumns;
}

void MainWindow::tableMergeSelectedColumns() {
	QList<int> selectedColumns = tableSelectedColumns();
	if (selectedColumns.size() <= 1) return;

	//Set field text to proper cell
	for (int i = selectedColumns.first() + 1; i <= selectedColumns.last(); i++) {
		if (table->item(2, selectedColumns.first())->text().isEmpty()) {
			table->item(2, selectedColumns.first())->setText(table->item(2, i)->text());
		}
		table->item(2, i)->setText("");

	}

	//Set or remove span
	if (table->columnSpan(2, selectedColumns.first()) > 1) {
		table->setSpan(1, selectedColumns.first(), 1, 1);
		table->setSpan(2, selectedColumns.first(), 1, 1);
	} else {
		table->setSpan(1, selectedColumns.first(), 1, abs(selectedColumns.last() - selectedColumns.first()) + 1);
		table->setSpan(2, selectedColumns.first(), 1, abs(selectedColumns.last() - selectedColumns.first()) + 1);
	}

	tableFieldValueRecalculate();
	onOutputUpdateRequest();
}

void MainWindow::onTableSelectChange() {
	bitSelectStatus->setText(QString::number(tableSelectedColumns().size()) + " bit");
}

void MainWindow::onColumnDeleteAction(bool checked) {
	QList<int> selectedColumns = tableSelectedColumns();

	for (int i = 0; i < selectedColumns.size(); i++) {
		table->removeColumn(selectedColumns.first());
	}

	if (table->columnCount() == 0) {
		for (int i = 0; i < 16; i++) {
			table->insertColumn(0);
		}

		tableItemsHeaderInit();
	}
}

void MainWindow::onColumnAddToLeftAction(bool checked) {
	QList<int> selectedColumns = tableSelectedColumns();

	for (int i = 0; i < selectedColumns.size(); i++) {
		table->insertColumn(selectedColumns.first());
	}

	tableItemsHeaderInit();
}

void MainWindow::onColumnAddToRightAction(bool checked) {
	QList<int> selectedColumns = tableSelectedColumns();

	for (int i = 0; i < selectedColumns.size(); i++) {
		table->insertColumn(selectedColumns.last() + 1);
	}

	tableItemsHeaderInit();
}

void MainWindow::onStorageSaveRequest(bool checked) {
	QString fileName = QDateTime::currentDateTime().toString("ddMMyy-hhmmss") + ".xml";
	QFile file(storageDir->absoluteFilePath(fileName));
	file.open(QFile::WriteOnly);

	QXmlStreamWriter xml(&file);
	xml.setAutoFormatting(true);
	xml.writeStartDocument();

	xml.writeStartElement("Table");
	xml.writeAttribute("cols", QString::number(ui->tableWidget->columnCount()));

	bool noFields = true;

	for (int i = 0 ;i < table->columnCount(); i++) {
		int colSpan = table->columnSpan(1, i);
		if (colSpan > 1) {
			xml.writeStartElement("Field");
			xml.writeAttribute("col", QString::number(i));
			xml.writeAttribute("span", QString::number(colSpan));
			xml.writeAttribute("value", table->item(1, i)->text());
			xml.writeCharacters(table->item(2, i)->text());
			xml.writeEndElement();

			i += colSpan - 1;
			noFields = false;
		}
	}

	xml.writeEndElement();
	xml.writeEndDocument();

	file.close();

	if (noFields) {
		storageDir->remove(fileName);
	}

	storageListUpdate();
}

void MainWindow::storageListUpdate() {
	QStringList files = storageDir->entryList(QStringList("*.xml"), QDir::Files);
	ui->STORAGE_LIST->clear();

	foreach (QString file, files) {
		file = file.replace(".xml", "");
		QListWidgetItem* item = new QListWidgetItem(file);
		item->setFlags(item->flags() | Qt::ItemIsEditable);
		ui->STORAGE_LIST->addItem(item);
	}
}

void MainWindow::onStorageRenameRequest(QListWidgetItem* item) {
	storageDir->rename(storageCurrentItemText + ".xml", item->text() + ".xml");
	storageListUpdate();
}

void MainWindow::onStorageSelectionChanged() {
	storageCurrentItemText = ui->STORAGE_LIST->currentItem()->text();
}

void MainWindow::onStorageLoadRequest(bool checked) {
	QString fileName = ui->STORAGE_LIST->currentItem()->text() + ".xml";
	QFile file(storageDir->absoluteFilePath(fileName));
	file.open(QFile::ReadOnly);

	table->setColumnCount(0);

	QXmlStreamReader xml(&file);

	while (!xml.atEnd() && !xml.hasError()) {
		QXmlStreamReader::TokenType token = xml.readNext();
		if (token == QXmlStreamReader::StartElement) {
			if (xml.name() == "Table") {
				table->setColumnCount(xml.attributes().value("cols").toInt());
				tableItemsHeaderInit();
			}
			if (xml.name() == "Field") {
				int col = xml.attributes().value("col").toInt();
				int span = xml.attributes().value("span").toInt();

				table->setSpan(1, col, 1, span);
				table->item(1, col)->setText(xml.attributes().value("value").toString());

				table->setSpan(2, col, 1, span);
				table->item(2, col)->setText(xml.readElementText());
			}
		}
	}

	file.close();
}
