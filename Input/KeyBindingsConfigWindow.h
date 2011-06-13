// For conditions of distribution and use, see copyright notice in license.txt

#pragma once

#include <QWidget>
#include <QKeySequence>
#include <map>
#include <string>

class QTreeWidget;
class QTreeWidgetItem;

class Framework;

class KeyBindingsConfigWindow : public QWidget
{
    Q_OBJECT

public:
    explicit KeyBindingsConfigWindow(Framework *fw) : framework(fw), configList(0)
    {
        ShowWindow();
    }

    // Creates and shows the configuration window.
    void ShowWindow();

    // Closes and deletes the configuration window.
    void CloseWindow();

public slots:
    void ApplyKeyConfig();
    void ButtonOK();
    void ButtonCancel();
    void ConfigListAdjustEditable(QTreeWidgetItem *item, int column);

private:
    void Clear();
    void PopulateBindingsList();
    void ExtractBindingsList();

    bool SpecialKeyPressChecker(int pressed_key);

    Framework *framework;

    QTreeWidget *configList;

    /// In the UI, the user edits values in this structure. When apply or OK is pressed, we update the real values to the input API.
    /// Edits are done here to allow Cancel to return without modifications having been done.
    std::map<std::string, QKeySequence> editedActions;

protected:
    bool eventFilter(QObject *obj, QEvent *event);
};

