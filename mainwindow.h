#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QTableWidget>
#include <QDir>
#include <QListWidgetItem>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	 ~MainWindow();

private slots:
	void onTableFieldValueUpdate(QTableWidgetItem* item);
	void onOutputUpdateRequest(int btnId = 0);
	void onTableCellDoubleClick(int row, int column);
	void onTableSelectChange();

	void onColumnDeleteAction(bool checked = false);
	void onColumnAddToRightAction(bool checked = false);
	void onColumnAddToLeftAction(bool checked = false);

	void onStorageSaveRequest(bool checked = false);
	void onStorageLoadRequest(bool checked = false);
	void onStorageRenameRequest(QListWidgetItem* item);
	void onStorageSelectionChanged();

private:
	Ui::MainWindow *ui;
	QString inputBits;
	QLabel* bitSelectStatus;
	QTableWidget* table;
	QDir* storageDir;
	QString storageCurrentItemText;

	bool eventFilter(QObject *watched, QEvent *event);
	void tableActionsSet();
	void tableItemsHeaderInit();
	void tableFieldValueRecalculate();
	void tableMergeSelectedColumns();
	void storageListUpdate();

	QString convertToBits(QString input, unsigned int base, bool* ok);
	QStringList tableBitsToChunks(int size);
	QList<int> tableSelectedColumns();
};

#endif // MAINWINDOW_H
