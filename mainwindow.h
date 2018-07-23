#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTableWidget>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	 ~MainWindow();

private slots:
	void onTableFieldValueUpdate(QTableWidgetItem* item);
	void onOutputUpdateRequest(int btnId = 0);
	void onTableFieldUpdate();
	void onCellDoubleClick(int row, int column);
	void onCellMergeRequest();
	void onSelectChange();

	void onColumnDeleteAction(bool checked = false);
	void onColumnAddToRightAction(bool checked = false);
	void onColumnAddToLeftAction(bool checked = false);


private:
	Ui::MainWindow *ui;
	QString inputBits;
	QLabel* bitSelectStatus;
	QTableWidget* table;

	bool eventFilter(QObject *watched, QEvent *event);
	void setTableActions();
	void initTable();

	QString convertToBits(QString input, unsigned int base, bool* ok);
	QStringList tableBitsToChunks(int size);
	QList<int> getSelectedColumns();
};

#endif // MAINWINDOW_H
