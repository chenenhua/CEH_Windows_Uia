#pragma once

#include "CustomDialog.h"
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QNetworkAccessManager>

class ModelSettingsDialog : public CustomDialog {
    Q_OBJECT
public:
    explicit ModelSettingsDialog(QWidget* parent = nullptr);

private slots:
    void onModeChanged(int index);
    void onSelectLocalModel();
    void onLocalModelTextChanged(const QString& text);
    void onTestConnection();
    void onSaveSettings();

private:
    void setupUi();
    void loadCurrentSettings();
    void updateModeVisibility();

    QComboBox* m_modeCombo{nullptr}; // 模式切换：本地 Ollama / 在线接口 vs 内置 llama.cpp 本地驱动
    QWidget* m_localModelContainer{nullptr};
    QLineEdit* m_localModelPathEdit{nullptr};
    QPushButton* m_btnSelectLocalModel{nullptr};

    // 本地硬件加速与引擎参数
    QSpinBox* m_gpuLayersSpin{nullptr};
    QSpinBox* m_threadsSpin{nullptr};

    // 远程/本地服务接口
    QLineEdit* m_endpointEdit{nullptr};
    QLineEdit* m_apiKeyEdit{nullptr};
    QLineEdit* m_modelNameEdit{nullptr};

    // 通用上下文与单次限额
    QSpinBox* m_contextTokensSpin{nullptr};
    QSpinBox* m_predictTokensSpin{nullptr};

    // 严谨采样控制
    QDoubleSpinBox* m_temperatureSpin{nullptr};
    QDoubleSpinBox* m_topPSpin{nullptr};
    QDoubleSpinBox* m_repeatPenaltySpin{nullptr};

    QPushButton* m_btnTest{nullptr};
    QLabel* m_testResultLabel{nullptr};

    QNetworkAccessManager* m_netManager{nullptr};
};
