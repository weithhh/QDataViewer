#include <QButtonGroup>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeyEvent>

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);

	QButtonGroup* inputTypeGroup = new QButtonGroup();
	inputTypeGroup->addButton(ui->IN_HEX);
	inputTypeGroup->addButton(ui->IN_BIN);
	inputTypeGroup->addButton(ui->IN_DEC);
	inputTypeGroup->setExclusive(true);

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

	ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	connect(ui->tableWidget, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSelectChange);
	connect(ui->tableWidget, &QTableWidget::cellDoubleClicked, this, &MainWindow::onCellDoubleClick);

	connect(ui->IN, &QLineEdit::returnPressed, this, &MainWindow::onInputUpdate);
	connect(ui->OUT_NOSPACE, &QPushButton::clicked, this, &MainWindow::onOutputConfigUpdate);

	connect(outputTypeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputConfigUpdate);
	connect(outputWidthGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputConfigUpdate);

	ui->tableWidget->installEventFilter(this);

	bitSelectStatus = new QLabel("0 bit");
	bitSelectStatus->setStyleSheet("padding: 5px");
	ui->statusBar->addPermanentWidget(bitSelectStatus);
}

MainWindow::~MainWindow(){
	delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
	if (watched == ui->tableWidget && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Space) {
			onCellMergeRequest();
			return true;
		}
	}
	return false;
}

QString MainWindow::convertToBits(QString input, unsigned int base, bool* ok) {
	QString bits;

	for (int i = 0; i < input.size(); i++) {
		QString charStr = QString(input.at(i));
		QString charBits = QString::number(charStr.toUInt(ok, base), 2);
		if (base != 2) {
			charBits = charBits.rightJustified(4, '0');
		}
		bits.append(charBits);
	}

	return bits;
}

void MainWindow::onInputUpdate() { //TODO: should save field names
	//Clean up whitespace
	QString inputText = ui->IN->text().remove(QRegExp("\\s"));
	if (ui->IN->text().isEmpty()) return;

	//Conversion of input value
	bool conversionSuccess;

	if (ui->IN_HEX->isChecked()) {
		inputText = inputText.remove("0x", Qt::CaseInsensitive);
		inputBits = convertToBits(inputText, 16, &conversionSuccess);
	} else if (ui->IN_BIN->isChecked()) {
		//Just to verify input
		inputBits = convertToBits(inputText, 2, &conversionSuccess);
	} else if (ui->IN_DEC->isChecked()) {
		inputBits = convertToBits(inputText, 10, &conversionSuccess);
	}

	if (!conversionSuccess) {
		ui->statusBar->showMessage("Failed to parse input value", 3000);
		return;
	} else {
		QString text = QString("Detected %1 bytes").arg((int)inputBits.size() / 8);
		if (inputBits.size() % 8 != 0) {
			text += QString(" and %1 bits").arg(inputBits.size() % 8);
		}
		ui->statusBar->showMessage(text, 3000);
	}

	//Table update
	ui->tableWidget->setColumnCount(inputBits.size());

	for (int i = 0, j = inputBits.size() - 1; i < inputBits.size(); i++, j--) {
		ui->tableWidget->setHorizontalHeaderItem(i, new QTableWidgetItem(QString::number(j)));

		QTableWidgetItem* valueItem = new QTableWidgetItem();
		valueItem->setTextAlignment(Qt::AlignCenter);
		valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
		valueItem->setText(inputBits.at(i));
		ui->tableWidget->setItem(0, i, valueItem);
		ui->tableWidget->setItem(1, i, valueItem->clone());

		QTableWidgetItem* nameItem = new QTableWidgetItem();
		nameItem->setTextAlignment(Qt::AlignCenter);
		ui->tableWidget->setItem(2, i, nameItem);
	}

	onOutputConfigUpdate();
}

void MainWindow::onTableFieldUpdate() {
	QTableWidget* table = ui->tableWidget;

	//Recalculate field values
	for (int i = 0; i < table->columnCount(); i++) {
		int colSpan = table->columnSpan(1, i);
		if (colSpan > 1) {
			QString fieldBits;
			for (int j = i; j < i + colSpan; j++) {
				fieldBits.append(table->item(0, j)->text());
			}

			quint64 fieldValue = fieldBits.toULongLong(nullptr, 2);
			QString fieldText = "0x" + QString::number(fieldValue, 16).rightJustified(ceil((float)colSpan / 4), '0').toUpper();
			table->item(1, i)->setText(fieldText);

			i += colSpan - 1;
		} else {
			table->item(1, i)->setText(table->item(0, i)->text());
		}
	}
}

QStringList MainWindow::tableBitsToChunks(int size) {
	QStringList chunks;
	QString chunk;

	for (int i = 0; i < ui->tableWidget->columnCount(); i++) {
		chunk.append(ui->tableWidget->item(0, i)->text());
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

void MainWindow::onOutputConfigUpdate(int btnId) {
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

void MainWindow::onCellDoubleClick(int row, int column) {
	if (row != 0) return;
	if (ui->tableWidget->item(0, column)->text() == "0") {
		ui->tableWidget->item(0, column)->setText("1");
	} else {
		ui->tableWidget->item(0, column)->setText("0");
	}

	onTableFieldUpdate();
	onOutputConfigUpdate();
}

QList<int> MainWindow::getSelectedColumns() {
	QTableWidget* table = ui->tableWidget;

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

void MainWindow::onCellMergeRequest() {
	QTableWidget* table = ui->tableWidget;

	QList<int> selectedColumns = getSelectedColumns();
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

	onTableFieldUpdate();
	onOutputConfigUpdate();
}

void MainWindow::onSelectChange() {
	bitSelectStatus->setText(QString::number(getSelectedColumns().size()) + " bit");
}
