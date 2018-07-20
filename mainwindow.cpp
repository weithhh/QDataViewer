#include <QButtonGroup>
#include <QTableWidget>
#include <QHeaderView>
#include <QKeyEvent>

#include "mainwindow.h"
#include "ui_mainwindow.h"

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

	table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

	bitSelectStatus = new QLabel("0 bit");
	bitSelectStatus->setStyleSheet("padding: 5px");
	ui->statusBar->addPermanentWidget(bitSelectStatus);

	for (int i = 0, j = table->columnCount() - 1; i < table->columnCount(); i++, j--) {
		table->setHorizontalHeaderItem(i, new QTableWidgetItem(QString::number(j)));

		QTableWidgetItem* bitItem = new QTableWidgetItem();
		bitItem->setTextAlignment(Qt::AlignCenter);
		bitItem->setFlags(bitItem->flags() & ~Qt::ItemIsEditable);
		bitItem->setText("0");
		table->setItem(0, i, bitItem);

		QTableWidgetItem* valueItem = new QTableWidgetItem();
		valueItem->setTextAlignment(Qt::AlignCenter);
		valueItem->setText("0");
		table->setItem(1, i, valueItem);

		QTableWidgetItem* nameItem = new QTableWidgetItem();
		nameItem->setTextAlignment(Qt::AlignCenter);
		table->setItem(2, i, nameItem);
	}

	onOutputUpdateRequest();

	connect(table, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSelectChange);
	connect(table, &QTableWidget::cellDoubleClicked, this, &MainWindow::onCellDoubleClick);
	connect(table, &QTableWidget::itemChanged, this, &MainWindow::onTableFieldValueUpdate);

	connect(ui->OUT_NOSPACE, &QPushButton::clicked, this, &MainWindow::onOutputUpdateRequest);

	connect(outputTypeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputUpdateRequest);
	connect(outputWidthGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &MainWindow::onOutputUpdateRequest);

	table->installEventFilter(this);

}

MainWindow::~MainWindow(){
	delete ui;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
	if (watched == table && event->type() == QEvent::KeyPress) {
		QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
		if (keyEvent->key() == Qt::Key_Space) {
			onCellMergeRequest();
			return true;
		}
		if (keyEvent->key() == Qt::Key_Delete) {
			QTableWidgetItem* item = table->currentItem();
			if (item->row() == 2) {
				item->setText("");
			}

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
		table->item(0, i + itemCol)->setText(QString(fieldBits.at(i)));
	}

	onOutputUpdateRequest();
}

void MainWindow::onTableFieldUpdate() {
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

	for (int i = 0; i < table->columnCount(); i++) {
		chunk.append(table->item(0, i)->text());
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

void MainWindow::onCellDoubleClick(int row, int column) {
	if (row != 0) return;

	if (table->item(0, column)->text() == "0") {
		table->item(0, column)->setText("1");
	} else {
		table->item(0, column)->setText("0");
	}	

	onTableFieldUpdate();
	onOutputUpdateRequest();
}

QList<int> MainWindow::getSelectedColumns() {
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
	onOutputUpdateRequest();
}

void MainWindow::onSelectChange() {
	bitSelectStatus->setText(QString::number(getSelectedColumns().size()) + " bit");
}
