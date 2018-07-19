#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

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
	void onInputUpdate();
	void onOutputConfigUpdate(int btnId = 0);
	void onTableFieldUpdate();
	void onCellDoubleClick(int row, int column);
	void onCellMergeRequest();
	void onSelectChange();

private:
	Ui::MainWindow *ui;
	QString inputBits;
	QLabel* bitSelectStatus;

	bool eventFilter(QObject *watched, QEvent *event);

	QString convertToBits(QString input, unsigned int base, bool* ok);
	QStringList tableBitsToChunks(int size);
	QList<int> getSelectedColumns();
};

#endif // MAINWINDOW_H
