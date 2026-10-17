// QtLite.h: the Qt 5.3 calls the host UI API does not wrap, bound at run time by mangled export name from
// Radiant's own Qt DLLs (Qt5Core64r / Qt5Gui64r / Qt5Widgets64r, MSVC 2012 x64). No Qt headers.
// Same technique as the host's src\qt.cpp: `this` in rcx, a by-value class return goes through a hidden pointer
// (after `this` for members, first for statics), QFlags travel as int. Main thread only.
//
// Everything is optional: Bind() resolves what exists, and each helper is a no-op (or returns a neutral value)
// when its export is missing, so a different Qt build costs features, never a crash.
//
// Objects that keep pointers into this DLL (event filters, signal slot objects, the syntax highlighter's vtable)
// are tracked and torn down by Shutdown(), which the plugin calls from OnUnload: the host frees the DLL right
// after, and Qt must never call into it again.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "GlyphRaster.h"

namespace ql
{
    using LogFn = void (*)(const char*);
    inline LogFn& LogSink() { static LogFn f = nullptr; return f; }
    inline void Log(const char* text) { if (LogSink()) LogSink()(text); }

    // Stack storage for a Qt value object (QColor, QPixmap, QIcon, QVariant...): generously sized and aligned.
    struct alignas(16) Buf { unsigned char b[64]; Buf() { memset(b, 0, sizeof b); } void* p() { return b; } const void* p() const { return b; } };
    struct QPoint { int x, y; };
    struct QSize { int w, h; };
    struct QRect { int x1, y1, x2, y2; };

    struct Api
    {
        bool bound = false, core = false;
        // CRT of the Qt build: objects Qt may delete must come from its heap
        void* (*opNew)(size_t) = nullptr;
        // QString
        void* (*QString_ctor)(void*, const char*) = nullptr;
        void  (*QString_dtor)(void*) = nullptr;
        void* (*QString_fromUtf16)(void* ret, const unsigned short*, int) = nullptr;
        void* (*QString_fromUtf8)(void* ret, const char*, int) = nullptr;   // Radiant's QString(const char*) reads Latin-1
        void  (*QWidget_setSizePolicy)(void*, int, int) = nullptr;
        void  (*QTreeWidgetItem_setText)(void*, int, const void*) = nullptr;
        void  (*QListWidgetItem_setText)(void*, const void*) = nullptr;
        // QObject
        void* (*QObject_ctor)(void*, void*) = nullptr;
        void  (*QObject_installEventFilter)(void*, void*) = nullptr;
        void  (*QObject_removeEventFilter)(void*, void*) = nullptr;
        void  (*QObject_deleteLater)(void*) = nullptr;
        bool  (*QObject_inherits)(const void*, const char*) = nullptr;
        void* (*QObject_connectImpl)(void* ret, const void* sender, void** signal, const void* receiver, void** slot, void* slotObj, int type, const int* types, const void* senderMeta) = nullptr;
        bool  (*QObject_disconnect)(const void* connection) = nullptr;
        void  (*Connection_dtor)(void*) = nullptr;
        // QVariant
        void* (*QVariant_ctorInt)(void*, int) = nullptr;
        void* (*QVariant_ctorString)(void*, const void*) = nullptr;
        void  (*QVariant_dtor)(void*) = nullptr;
        // QApplication
        void* (*QApplication_focusWidget)() = nullptr;
        // QColor / QBrush / QPixmap / QPainter / QIcon / QFont / QCursor
        void* (*QColor_ctor)(void*, int, int, int, int) = nullptr;
        void* (*QBrush_ctorColor)(void*, const void*, int) = nullptr;
        void  (*QBrush_dtor)(void*) = nullptr;
        void* (*QPixmap_ctor)(void*, int, int) = nullptr;
        void  (*QPixmap_dtor)(void*) = nullptr;
        void  (*QPixmap_fill)(void*, const void*) = nullptr;
        bool  (*QPixmap_convertFromImage)(void*, const void*, int) = nullptr;
        void* (*QImage_ctor)(void*, int, int, int) = nullptr;   // (w, h, format): owns its pixels
        unsigned char* (*QImage_bits)(void*) = nullptr;
        int   (*QImage_bytesPerLine)(const void*) = nullptr;
        void  (*QImage_dtor)(void*) = nullptr;
        void* (*QPainter_ctor)(void*, void*) = nullptr;
        void  (*QPainter_dtor)(void*) = nullptr;
        void  (*QPainter_setRenderHint)(void*, int, bool) = nullptr;
        void  (*QPainter_setPenStyle)(void*, int) = nullptr;
        void  (*QPainter_setPenColor)(void*, const void*) = nullptr;
        void  (*QPainter_setBrush)(void*, const void*) = nullptr;
        void  (*QPainter_drawEllipse)(void*, int, int, int, int) = nullptr;
        void  (*QPainter_drawRoundedRect)(void*, int, int, int, int, double, double, int) = nullptr;
        bool  (*QPainter_end)(void*) = nullptr;
        void  (*QPainter_drawLine)(void*, int, int, int, int) = nullptr;
        void  (*QAbstractButton_setIcon)(void*, const void*) = nullptr;
        void* (*QIcon_ctorPixmap)(void*, const void*) = nullptr;
        void  (*QIcon_dtor)(void*) = nullptr;
        void* (*QIcon_toVariant)(const void*, void* ret) = nullptr;
        void* (*QCursor_pos)(void* ret) = nullptr;
        void* (*QFont_ctor)(void*, const void*, int, int, bool) = nullptr;
        void  (*QFont_dtor)(void*) = nullptr;
        void  (*QFont_setPointSize)(void*, int) = nullptr;
        void* (*QFont_toVariant)(const void*, void* ret) = nullptr;
        void* (*QFontMetrics_ctor)(void*, const void*) = nullptr;
        void  (*QFontMetrics_dtor)(void*) = nullptr;
        int   (*QFontMetrics_averageCharWidth)(const void*) = nullptr;
        // QWidget / scroll areas / item views
        void  (*QWidget_setFont)(void*, const void*) = nullptr;
        void  (*QWidget_setUpdatesEnabled)(void*, bool) = nullptr;
        void  (*QWidget_setStyleSheet)(void*, const void*) = nullptr;
        void  (*QWidget_setFixedWidth)(void*, int) = nullptr;
        void  (*QWidget_setFixedHeight)(void*, int) = nullptr;
        void* (*QWidget_mapToGlobal)(const void*, void* ret, const void* pos) = nullptr;
        void* (*QAbstractScrollArea_viewport)(const void*) = nullptr;
        void* (*QAbstractScrollArea_verticalScrollBar)(const void*) = nullptr;
        int   (*QAbstractSlider_value)(const void*) = nullptr;
        void  (*QAbstractSlider_setValue)(void*, int) = nullptr;
        void  (*QAbstractItemView_setDragEnabled)(void*, bool) = nullptr;
        void  (*QAbstractItemView_setDragDropMode)(void*, int) = nullptr;
        void  (*QAbstractItemView_setDefaultDropAction)(void*, int) = nullptr;
        void  (*QAbstractItemView_setIconSize)(void*, const void*) = nullptr;
        void  (*QAbstractItemView_setSelectionMode)(void*, int) = nullptr;
        void  (*QAbstractItemView_setTextElideMode)(void*, int) = nullptr;
        void  (*QAbstractItemView_stopAutoScroll)(void*) = nullptr;
        void* (*QTreeView_header)(const void*) = nullptr;
        void  (*QTreeView_setIndentation)(void*, int) = nullptr;
        void  (*QTreeView_setExpandsOnDoubleClick)(void*, bool) = nullptr;
        void  (*QTreeView_setRootIsDecorated)(void*, bool) = nullptr;
        void  (*QHeaderView_setSectionResizeMode)(void*, int, int) = nullptr;
        void  (*QHeaderView_setStretchLastSection)(void*, bool) = nullptr;
        void* (*QTreeWidget_currentItem)(const void*) = nullptr;
        void  (*QTreeWidget_setCurrentItem)(void*, void*) = nullptr;
        void* (*QTreeWidget_itemAt)(const void*, int, int) = nullptr;
        void  (*QTreeWidget_scrollToItem)(void*, const void*, int) = nullptr;
        void  (*QTreeWidgetItem_setData)(void*, int, int, const void*) = nullptr;
        void  (*QTreeWidgetItem_setCheckState)(void*, int, int) = nullptr;
        int   (*QTreeWidgetItem_checkState)(const void*, int) = nullptr;
        void  (*QTreeWidgetItem_setHidden)(void*, bool) = nullptr;
        bool  (*QTreeWidgetItem_isExpanded)(const void*) = nullptr;
        void* (*QTreeWidgetItem_flags)(const void*, int* ret) = nullptr;
        void  (*QTreeWidgetItem_setFlags)(void*, int) = nullptr;
        void  (*QTreeWidgetItem_setForeground)(void*, int, const void*) = nullptr;
        void* (*QListWidget_item)(const void*, int) = nullptr;
        void* (*QListWidget_itemAt)(const void*, int, int) = nullptr;
        void  (*QListWidgetItem_setData)(void*, int, const void*) = nullptr;
        void  (*QListView_setViewMode)(void*, int) = nullptr;
        void  (*QListView_setGridSize)(void*, const void*) = nullptr;
        void  (*QListView_setResizeMode)(void*, int) = nullptr;
        void  (*QListView_setWrapping)(void*, bool) = nullptr;
        void  (*QListView_setMovement)(void*, int) = nullptr;
        void  (*QListView_setUniformItemSizes)(void*, bool) = nullptr;
        void  (*QListView_setWordWrap)(void*, bool) = nullptr;
        void  (*QListView_setSpacing)(void*, int) = nullptr;
        // menus and dialogs
        void* (*QMenu_ctor)(void*, void*) = nullptr;
        void* (*QMenu_addAction)(void*, const void*) = nullptr;
        void* (*QMenu_addSeparator)(void*) = nullptr;
        void* (*QMenu_exec)(void*, const void*, void*) = nullptr;
        void  (*QAction_setCheckable)(void*, bool) = nullptr;
        void  (*QAction_setChecked)(void*, bool) = nullptr;
        void  (*QAction_setEnabled)(void*, bool) = nullptr;
        void* (*QInputDialog_getText)(void* ret, void* parent, const void* title, const void* label, int echo, const void* text, bool* ok, int flags, int hints) = nullptr;
        int   (*QInputDialog_getInt)(void* parent, const void* title, const void* label, int value, int minValue, int maxValue, int step, bool* ok, int flags) = nullptr;
        int   (*QMessageBox_question)(void* parent, const void* title, const void* text, int buttons, int defaultButton) = nullptr;
        int   (*QMessageBox_warning)(void* parent, const void* title, const void* text, int buttons, int defaultButton) = nullptr;
        // plain text editing
        void* (*QPlainTextEdit_toPlainText)(const void*, void* ret) = nullptr;
        void  (*QPlainTextEdit_setPlainText)(void*, const void*) = nullptr;
        void* (*QPlainTextEdit_document)(const void*) = nullptr;
        void* (*QPlainTextEdit_textCursor)(const void*, void* ret) = nullptr;
        void  (*QPlainTextEdit_setTextCursor)(void*, const void*) = nullptr;
        void  (*QPlainTextEdit_setLineWrapMode)(void*, int) = nullptr;
        void  (*QPlainTextEdit_setTabStopWidth)(void*, int) = nullptr;
        bool  (*QPlainTextEdit_find)(void*, const void*, int) = nullptr;
        void  (*QPlainTextEdit_centerCursor)(void*) = nullptr;
        void* (*QPlainTextEdit_cursorRect)(const void*, void* ret) = nullptr;
        void  (*QPlainTextEdit_insertPlainText)(void*, const void*) = nullptr;
        bool  (*QTextDocument_isModified)(const void*) = nullptr;
        void  (*QTextDocument_setModified)(void*, bool) = nullptr;
        void* (*QTextDocument_findBlockByNumber)(const void*, void* ret, int) = nullptr;
        int   (*QTextDocument_blockCount)(const void*) = nullptr;
        int   (*QTextBlock_position)(const void*) = nullptr;
        void* (*QTextCursor_ctorBlock)(void*, const void*) = nullptr;
        void  (*QTextCursor_dtor)(void*) = nullptr;
        int   (*QTextCursor_position)(const void*) = nullptr;
        int   (*QTextCursor_selectionStart)(const void*) = nullptr;
        int   (*QTextCursor_selectionEnd)(const void*) = nullptr;
        bool  (*QTextCursor_hasSelection)(const void*) = nullptr;
        void  (*QTextCursor_setPosition)(void*, int, int) = nullptr;
        void  (*QTextCursor_insertText)(void*, const void*) = nullptr;
        void  (*QTextCursor_removeSelectedText)(void*) = nullptr;
        void  (*QTextCursor_beginEditBlock)(void*) = nullptr;
        void  (*QTextCursor_endEditBlock)(void*) = nullptr;
        int   (*QTextCursor_blockNumber)(const void*) = nullptr;
        int   (*QTextCursor_positionInBlock)(const void*) = nullptr;
        void* (*QTextCursor_selectedText)(const void*, void* ret) = nullptr;
        // tabs
        void  (*QTabWidget_setTabsClosable)(void*, bool) = nullptr;
        void  (*QTabWidget_removeTab)(void*, int) = nullptr;
        void  (*QTabWidget_setTabText)(void*, int, const void*) = nullptr;
        void  (*QTabWidget_setTabToolTip)(void*, int, const void*) = nullptr;
        int   (*QTabWidget_indexOf)(const void*, void*) = nullptr;
        void* (*QTabWidget_widget)(const void*, int) = nullptr;
        int   (*QTabWidget_count)(const void*) = nullptr;
        void  (*QTabWidget_setCurrentWidget)(void*, void*) = nullptr;
        void  (*QTabWidget_setMovable)(void*, bool) = nullptr;
        void  (*QTabWidget_setDocumentMode)(void*, bool) = nullptr;
        // signals (member-function addresses) and their classes' meta objects
        void* sig_QTabWidget_tabCloseRequested = nullptr; const void* meta_QTabWidget = nullptr;
        void* sig_QTreeWidget_itemSelectionChanged = nullptr; const void* meta_QTreeWidget = nullptr;
        // multi-selection in trees
        void* (*QTreeWidget_selectedItems)(const void*, void* ret) = nullptr;
        void  (*QTreeWidgetItem_setSelected)(void*, bool) = nullptr;
        void  (*QAbstractItemView_clearSelection)(void*) = nullptr;
        void  (*QListData_dispose)(void*) = nullptr;
        // syntax highlighter
        void* (*QSyntaxHighlighter_ctor)(void*, void*) = nullptr;
        void  (*QSyntaxHighlighter_setFormat)(void*, int, int, const void*) = nullptr;
        int   (*QSyntaxHighlighter_previousBlockState)(const void*) = nullptr;
        void  (*QSyntaxHighlighter_setCurrentBlockState)(void*, int) = nullptr;
        void  (*QSyntaxHighlighter_setDocument)(void*, void*) = nullptr;
        void  (*QSyntaxHighlighter_rehighlight)(void*) = nullptr;
        void** QSyntaxHighlighter_vtable = nullptr;
        void*  QSyntaxHighlighter_metaObject = nullptr;
    };
    inline Api& A() { static Api a; return a; }

    template <typename T> bool Res(T& out, const wchar_t* dll, const char* name)
    {
        HMODULE m = GetModuleHandleW(dll);
        out = m ? reinterpret_cast<T>(GetProcAddress(m, name)) : nullptr;
        if (!out) { std::string t = "QtLite: missing export "; t += name; Log(t.c_str()); }
        return out != nullptr;
    }

    inline bool Bind()
    {
        Api& a = A();
        if (a.bound) return a.core;
        a.bound = true;
        const wchar_t* C = L"Qt5Core64r.dll"; const wchar_t* G = L"Qt5Gui64r.dll"; const wchar_t* W = L"Qt5Widgets64r.dll";
        bool core = true;
        core &= Res(a.opNew, L"msvcr110.dll", "??2@YAPEAX_K@Z");
        core &= Res(a.QString_ctor, C, "??0QString@@QEAA@PEBD@Z");
        core &= Res(a.QString_dtor, C, "??1QString@@QEAA@XZ");
        core &= Res(a.QString_fromUtf16, C, "?fromUtf16@QString@@SA?AV1@PEBGH@Z");
        core &= Res(a.QString_fromUtf8, C, "?fromUtf8@QString@@SA?AV1@PEBDH@Z");
        Res(a.QWidget_setSizePolicy, W, "?setSizePolicy@QWidget@@QEAAXW4Policy@QSizePolicy@@0@Z");
        Res(a.QTreeWidgetItem_setText, W, "?setText@QTreeWidgetItem@@QEAAXHAEBVQString@@@Z");
        Res(a.QListWidgetItem_setText, W, "?setText@QListWidgetItem@@QEAAXAEBVQString@@@Z");
        core &= Res(a.QObject_ctor, C, "??0QObject@@QEAA@PEAV0@@Z");
        core &= Res(a.QObject_installEventFilter, C, "?installEventFilter@QObject@@QEAAXPEAV1@@Z");
        core &= Res(a.QObject_removeEventFilter, C, "?removeEventFilter@QObject@@QEAAXPEAV1@@Z");
        core &= Res(a.QObject_deleteLater, C, "?deleteLater@QObject@@QEAAXXZ");
        Res(a.QObject_inherits, C, "?inherits@QObject@@QEBA_NPEBD@Z");
        Res(a.QObject_connectImpl, C, "?connectImpl@QObject@@CA?AVConnection@QMetaObject@@PEBV1@PEAPEAX01PEAVQSlotObjectBase@QtPrivate@@W4ConnectionType@Qt@@PEBHPEBU3@@Z");
        Res(a.QObject_disconnect, C, "?disconnect@QObject@@SA_NAEBVConnection@QMetaObject@@@Z");
        Res(a.Connection_dtor, C, "??1Connection@QMetaObject@@QEAA@XZ");
        core &= Res(a.QVariant_ctorInt, C, "??0QVariant@@QEAA@H@Z");
        core &= Res(a.QVariant_ctorString, C, "??0QVariant@@QEAA@AEBVQString@@@Z");
        core &= Res(a.QVariant_dtor, C, "??1QVariant@@QEAA@XZ");
        Res(a.QApplication_focusWidget, W, "?focusWidget@QApplication@@SAPEAVQWidget@@XZ");

        Res(a.QColor_ctor, G, "??0QColor@@QEAA@HHHH@Z");
        Res(a.QBrush_ctorColor, G, "??0QBrush@@QEAA@AEBVQColor@@W4BrushStyle@Qt@@@Z");
        Res(a.QBrush_dtor, G, "??1QBrush@@QEAA@XZ");
        Res(a.QPixmap_ctor, G, "??0QPixmap@@QEAA@HH@Z");
        Res(a.QPixmap_dtor, G, "??1QPixmap@@UEAA@XZ");
        Res(a.QPixmap_fill, G, "?fill@QPixmap@@QEAAXAEBVQColor@@@Z");
        Res(a.QPixmap_convertFromImage, G, "?convertFromImage@QPixmap@@QEAA_NAEBVQImage@@V?$QFlags@W4ImageConversionFlag@Qt@@@@@Z");
        Res(a.QImage_ctor, G, "??0QImage@@QEAA@HHW4Format@0@@Z");
        Res(a.QImage_bits, G, "?bits@QImage@@QEAAPEAEXZ");
        Res(a.QImage_bytesPerLine, G, "?bytesPerLine@QImage@@QEBAHXZ");
        Res(a.QImage_dtor, G, "??1QImage@@UEAA@XZ");
        Res(a.QPainter_ctor, G, "??0QPainter@@QEAA@PEAVQPaintDevice@@@Z");
        Res(a.QPainter_dtor, G, "??1QPainter@@QEAA@XZ");
        Res(a.QPainter_setRenderHint, G, "?setRenderHint@QPainter@@QEAAXW4RenderHint@1@_N@Z");
        Res(a.QPainter_setPenStyle, G, "?setPen@QPainter@@QEAAXW4PenStyle@Qt@@@Z");
        Res(a.QPainter_setPenColor, G, "?setPen@QPainter@@QEAAXAEBVQColor@@@Z");
        Res(a.QPainter_setBrush, G, "?setBrush@QPainter@@QEAAXAEBVQBrush@@@Z");
        Res(a.QPainter_drawEllipse, G, "?drawEllipse@QPainter@@QEAAXHHHH@Z");
        Res(a.QPainter_drawRoundedRect, G, "?drawRoundedRect@QPainter@@QEAAXHHHHNNW4SizeMode@Qt@@@Z");
        Res(a.QPainter_end, G, "?end@QPainter@@QEAA_NXZ");
        Res(a.QPainter_drawLine, G, "?drawLine@QPainter@@QEAAXHHHH@Z");
        Res(a.QAbstractButton_setIcon, W, "?setIcon@QAbstractButton@@QEAAXAEBVQIcon@@@Z");
        Res(a.QIcon_ctorPixmap, G, "??0QIcon@@QEAA@AEBVQPixmap@@@Z");
        Res(a.QIcon_dtor, G, "??1QIcon@@QEAA@XZ");
        Res(a.QIcon_toVariant, G, "??BQIcon@@QEBA?AVQVariant@@XZ");
        Res(a.QCursor_pos, G, "?pos@QCursor@@SA?AVQPoint@@XZ");
        Res(a.QFont_ctor, G, "??0QFont@@QEAA@AEBVQString@@HH_N@Z");
        Res(a.QFont_dtor, G, "??1QFont@@QEAA@XZ");
        Res(a.QFont_setPointSize, G, "?setPointSize@QFont@@QEAAXH@Z");
        Res(a.QFont_toVariant, G, "??BQFont@@QEBA?AVQVariant@@XZ");
        Res(a.QFontMetrics_ctor, G, "??0QFontMetrics@@QEAA@AEBVQFont@@@Z");
        Res(a.QFontMetrics_dtor, G, "??1QFontMetrics@@QEAA@XZ");
        Res(a.QFontMetrics_averageCharWidth, G, "?averageCharWidth@QFontMetrics@@QEBAHXZ");

        Res(a.QWidget_setFont, W, "?setFont@QWidget@@QEAAXAEBVQFont@@@Z");
        Res(a.QWidget_setUpdatesEnabled, W, "?setUpdatesEnabled@QWidget@@QEAAX_N@Z");
        Res(a.QWidget_setStyleSheet, W, "?setStyleSheet@QWidget@@QEAAXAEBVQString@@@Z");
        Res(a.QWidget_setFixedWidth, W, "?setFixedWidth@QWidget@@QEAAXH@Z");
        Res(a.QWidget_setFixedHeight, W, "?setFixedHeight@QWidget@@QEAAXH@Z");
        Res(a.QWidget_mapToGlobal, W, "?mapToGlobal@QWidget@@QEBA?AVQPoint@@AEBV2@@Z");
        Res(a.QAbstractScrollArea_viewport, W, "?viewport@QAbstractScrollArea@@QEBAPEAVQWidget@@XZ");
        Res(a.QAbstractScrollArea_verticalScrollBar, W, "?verticalScrollBar@QAbstractScrollArea@@QEBAPEAVQScrollBar@@XZ");
        Res(a.QAbstractSlider_value, W, "?value@QAbstractSlider@@QEBAHXZ");
        Res(a.QAbstractSlider_setValue, W, "?setValue@QAbstractSlider@@QEAAXH@Z");
        Res(a.QAbstractItemView_setDragEnabled, W, "?setDragEnabled@QAbstractItemView@@QEAAX_N@Z");
        Res(a.QAbstractItemView_setDragDropMode, W, "?setDragDropMode@QAbstractItemView@@QEAAXW4DragDropMode@1@@Z");
        Res(a.QAbstractItemView_setDefaultDropAction, W, "?setDefaultDropAction@QAbstractItemView@@QEAAXW4DropAction@Qt@@@Z");
        Res(a.QAbstractItemView_setIconSize, W, "?setIconSize@QAbstractItemView@@QEAAXAEBVQSize@@@Z");
        Res(a.QAbstractItemView_setSelectionMode, W, "?setSelectionMode@QAbstractItemView@@QEAAXW4SelectionMode@1@@Z");
        Res(a.QAbstractItemView_setTextElideMode, W, "?setTextElideMode@QAbstractItemView@@QEAAXW4TextElideMode@Qt@@@Z");
        Res(a.QAbstractItemView_stopAutoScroll, W, "?stopAutoScroll@QAbstractItemView@@IEAAXXZ");
        Res(a.QTreeView_header, W, "?header@QTreeView@@QEBAPEAVQHeaderView@@XZ");
        Res(a.QTreeView_setIndentation, W, "?setIndentation@QTreeView@@QEAAXH@Z");
        Res(a.QTreeView_setExpandsOnDoubleClick, W, "?setExpandsOnDoubleClick@QTreeView@@QEAAX_N@Z");
        Res(a.QTreeView_setRootIsDecorated, W, "?setRootIsDecorated@QTreeView@@QEAAX_N@Z");
        Res(a.QHeaderView_setSectionResizeMode, W, "?setSectionResizeMode@QHeaderView@@QEAAXHW4ResizeMode@1@@Z");
        Res(a.QHeaderView_setStretchLastSection, W, "?setStretchLastSection@QHeaderView@@QEAAX_N@Z");
        Res(a.QTreeWidget_currentItem, W, "?currentItem@QTreeWidget@@QEBAPEAVQTreeWidgetItem@@XZ");
        Res(a.QTreeWidget_setCurrentItem, W, "?setCurrentItem@QTreeWidget@@QEAAXPEAVQTreeWidgetItem@@@Z");
        Res(a.QTreeWidget_itemAt, W, "?itemAt@QTreeWidget@@QEBAPEAVQTreeWidgetItem@@HH@Z");
        Res(a.QTreeWidget_scrollToItem, W, "?scrollToItem@QTreeWidget@@QEAAXPEBVQTreeWidgetItem@@W4ScrollHint@QAbstractItemView@@@Z");
        Res(a.QTreeWidgetItem_setData, W, "?setData@QTreeWidgetItem@@UEAAXHHAEBVQVariant@@@Z");
        Res(a.QTreeWidgetItem_setCheckState, W, "?setCheckState@QTreeWidgetItem@@QEAAXHW4CheckState@Qt@@@Z");
        Res(a.QTreeWidgetItem_checkState, W, "?checkState@QTreeWidgetItem@@QEBA?AW4CheckState@Qt@@H@Z");
        Res(a.QTreeWidgetItem_setHidden, W, "?setHidden@QTreeWidgetItem@@QEAAX_N@Z");
        Res(a.QTreeWidgetItem_isExpanded, W, "?isExpanded@QTreeWidgetItem@@QEBA_NXZ");
        Res(a.QTreeWidgetItem_flags, W, "?flags@QTreeWidgetItem@@QEBA?AV?$QFlags@W4ItemFlag@Qt@@@@XZ");
        Res(a.QTreeWidgetItem_setFlags, W, "?setFlags@QTreeWidgetItem@@QEAAXV?$QFlags@W4ItemFlag@Qt@@@@@Z");
        Res(a.QTreeWidgetItem_setForeground, W, "?setForeground@QTreeWidgetItem@@QEAAXHAEBVQBrush@@@Z");
        Res(a.QListWidget_item, W, "?item@QListWidget@@QEBAPEAVQListWidgetItem@@H@Z");
        Res(a.QListWidget_itemAt, W, "?itemAt@QListWidget@@QEBAPEAVQListWidgetItem@@HH@Z");
        Res(a.QListWidgetItem_setData, W, "?setData@QListWidgetItem@@UEAAXHAEBVQVariant@@@Z");
        Res(a.QListView_setViewMode, W, "?setViewMode@QListView@@QEAAXW4ViewMode@1@@Z");
        Res(a.QListView_setGridSize, W, "?setGridSize@QListView@@QEAAXAEBVQSize@@@Z");
        Res(a.QListView_setResizeMode, W, "?setResizeMode@QListView@@QEAAXW4ResizeMode@1@@Z");
        Res(a.QListView_setWrapping, W, "?setWrapping@QListView@@QEAAX_N@Z");
        Res(a.QListView_setMovement, W, "?setMovement@QListView@@QEAAXW4Movement@1@@Z");
        Res(a.QListView_setUniformItemSizes, W, "?setUniformItemSizes@QListView@@QEAAX_N@Z");
        Res(a.QListView_setWordWrap, W, "?setWordWrap@QListView@@QEAAX_N@Z");
        Res(a.QListView_setSpacing, W, "?setSpacing@QListView@@QEAAXH@Z");

        Res(a.QMenu_ctor, W, "??0QMenu@@QEAA@PEAVQWidget@@@Z");
        Res(a.QMenu_addAction, W, "?addAction@QMenu@@QEAAPEAVQAction@@AEBVQString@@@Z");
        Res(a.QMenu_addSeparator, W, "?addSeparator@QMenu@@QEAAPEAVQAction@@XZ");
        Res(a.QMenu_exec, W, "?exec@QMenu@@QEAAPEAVQAction@@AEBVQPoint@@PEAV2@@Z");
        Res(a.QAction_setCheckable, W, "?setCheckable@QAction@@QEAAX_N@Z");
        Res(a.QAction_setChecked, W, "?setChecked@QAction@@QEAAX_N@Z");
        Res(a.QAction_setEnabled, W, "?setEnabled@QAction@@QEAAX_N@Z");
        Res(a.QInputDialog_getText, W, "?getText@QInputDialog@@SA?AVQString@@PEAVQWidget@@AEBV2@1W4EchoMode@QLineEdit@@1PEA_NV?$QFlags@W4WindowType@Qt@@@@V?$QFlags@W4InputMethodHint@Qt@@@@@Z");
        Res(a.QInputDialog_getInt, W, "?getInt@QInputDialog@@SAHPEAVQWidget@@AEBVQString@@1HHHHPEA_NV?$QFlags@W4WindowType@Qt@@@@@Z");
        Res(a.QMessageBox_question, W, "?question@QMessageBox@@SA?AW4StandardButton@1@PEAVQWidget@@AEBVQString@@1V?$QFlags@W4StandardButton@QMessageBox@@@@W421@@Z");
        Res(a.QMessageBox_warning, W, "?warning@QMessageBox@@SA?AW4StandardButton@1@PEAVQWidget@@AEBVQString@@1V?$QFlags@W4StandardButton@QMessageBox@@@@W421@@Z");

        Res(a.QPlainTextEdit_toPlainText, W, "?toPlainText@QPlainTextEdit@@QEBA?AVQString@@XZ");
        Res(a.QPlainTextEdit_setPlainText, W, "?setPlainText@QPlainTextEdit@@QEAAXAEBVQString@@@Z");
        Res(a.QPlainTextEdit_document, W, "?document@QPlainTextEdit@@QEBAPEAVQTextDocument@@XZ");
        Res(a.QPlainTextEdit_textCursor, W, "?textCursor@QPlainTextEdit@@QEBA?AVQTextCursor@@XZ");
        Res(a.QPlainTextEdit_setTextCursor, W, "?setTextCursor@QPlainTextEdit@@QEAAXAEBVQTextCursor@@@Z");
        Res(a.QPlainTextEdit_setLineWrapMode, W, "?setLineWrapMode@QPlainTextEdit@@QEAAXW4LineWrapMode@1@@Z");
        Res(a.QPlainTextEdit_setTabStopWidth, W, "?setTabStopWidth@QPlainTextEdit@@QEAAXH@Z");
        Res(a.QPlainTextEdit_find, W, "?find@QPlainTextEdit@@QEAA_NAEBVQString@@V?$QFlags@W4FindFlag@QTextDocument@@@@@Z");
        Res(a.QPlainTextEdit_centerCursor, W, "?centerCursor@QPlainTextEdit@@QEAAXXZ");
        Res(a.QPlainTextEdit_cursorRect, W, "?cursorRect@QPlainTextEdit@@QEBA?AVQRect@@XZ");
        Res(a.QPlainTextEdit_insertPlainText, W, "?insertPlainText@QPlainTextEdit@@QEAAXAEBVQString@@@Z");
        Res(a.QTextDocument_isModified, G, "?isModified@QTextDocument@@QEBA_NXZ");
        Res(a.QTextDocument_setModified, G, "?setModified@QTextDocument@@QEAAX_N@Z");
        Res(a.QTextDocument_findBlockByNumber, G, "?findBlockByNumber@QTextDocument@@QEBA?AVQTextBlock@@H@Z");
        Res(a.QTextDocument_blockCount, G, "?blockCount@QTextDocument@@QEBAHXZ");
        Res(a.QTextBlock_position, G, "?position@QTextBlock@@QEBAHXZ");
        Res(a.QTextCursor_ctorBlock, G, "??0QTextCursor@@QEAA@AEBVQTextBlock@@@Z");
        Res(a.QTextCursor_dtor, G, "??1QTextCursor@@QEAA@XZ");
        Res(a.QTextCursor_position, G, "?position@QTextCursor@@QEBAHXZ");
        Res(a.QTextCursor_selectionStart, G, "?selectionStart@QTextCursor@@QEBAHXZ");
        Res(a.QTextCursor_selectionEnd, G, "?selectionEnd@QTextCursor@@QEBAHXZ");
        Res(a.QTextCursor_hasSelection, G, "?hasSelection@QTextCursor@@QEBA_NXZ");
        Res(a.QTextCursor_setPosition, G, "?setPosition@QTextCursor@@QEAAXHW4MoveMode@1@@Z");
        Res(a.QTextCursor_insertText, G, "?insertText@QTextCursor@@QEAAXAEBVQString@@@Z");
        Res(a.QTextCursor_removeSelectedText, G, "?removeSelectedText@QTextCursor@@QEAAXXZ");
        Res(a.QTextCursor_beginEditBlock, G, "?beginEditBlock@QTextCursor@@QEAAXXZ");
        Res(a.QTextCursor_endEditBlock, G, "?endEditBlock@QTextCursor@@QEAAXXZ");
        Res(a.QTextCursor_blockNumber, G, "?blockNumber@QTextCursor@@QEBAHXZ");
        Res(a.QTextCursor_positionInBlock, G, "?positionInBlock@QTextCursor@@QEBAHXZ");
        Res(a.QTextCursor_selectedText, G, "?selectedText@QTextCursor@@QEBA?AVQString@@XZ");

        Res(a.QTabWidget_setTabsClosable, W, "?setTabsClosable@QTabWidget@@QEAAX_N@Z");
        Res(a.QTabWidget_removeTab, W, "?removeTab@QTabWidget@@QEAAXH@Z");
        Res(a.QTabWidget_setTabText, W, "?setTabText@QTabWidget@@QEAAXHAEBVQString@@@Z");
        Res(a.QTabWidget_setTabToolTip, W, "?setTabToolTip@QTabWidget@@QEAAXHAEBVQString@@@Z");
        Res(a.QTabWidget_indexOf, W, "?indexOf@QTabWidget@@QEBAHPEAVQWidget@@@Z");
        Res(a.QTabWidget_widget, W, "?widget@QTabWidget@@QEBAPEAVQWidget@@H@Z");
        Res(a.QTabWidget_count, W, "?count@QTabWidget@@QEBAHXZ");
        Res(a.QTabWidget_setCurrentWidget, W, "?setCurrentWidget@QTabWidget@@QEAAXPEAVQWidget@@@Z");
        Res(a.QTabWidget_setMovable, W, "?setMovable@QTabWidget@@QEAAX_N@Z");
        Res(a.QTabWidget_setDocumentMode, W, "?setDocumentMode@QTabWidget@@QEAAX_N@Z");
        Res(a.sig_QTabWidget_tabCloseRequested, W, "?tabCloseRequested@QTabWidget@@QEAAXH@Z");
        Res(a.meta_QTabWidget, W, "?staticMetaObject@QTabWidget@@2UQMetaObject@@B");
        Res(a.sig_QTreeWidget_itemSelectionChanged, W, "?itemSelectionChanged@QTreeWidget@@QEAAXXZ");
        Res(a.meta_QTreeWidget, W, "?staticMetaObject@QTreeWidget@@2UQMetaObject@@B");
        Res(a.QTreeWidget_selectedItems, W, "?selectedItems@QTreeWidget@@QEBA?AV?$QList@PEAVQTreeWidgetItem@@@@XZ");
        Res(a.QTreeWidgetItem_setSelected, W, "?setSelected@QTreeWidgetItem@@QEAAX_N@Z");
        Res(a.QAbstractItemView_clearSelection, W, "?clearSelection@QAbstractItemView@@QEAAXXZ");
        Res(a.QListData_dispose, C, "?dispose@QListData@@SAXPEAUData@1@@Z");

        Res(a.QSyntaxHighlighter_ctor, G, "??0QSyntaxHighlighter@@QEAA@PEAVQTextDocument@@@Z");
        Res(a.QSyntaxHighlighter_setFormat, G, "?setFormat@QSyntaxHighlighter@@IEAAXHHAEBVQColor@@@Z");
        Res(a.QSyntaxHighlighter_previousBlockState, G, "?previousBlockState@QSyntaxHighlighter@@IEBAHXZ");
        Res(a.QSyntaxHighlighter_setCurrentBlockState, G, "?setCurrentBlockState@QSyntaxHighlighter@@IEAAXH@Z");
        Res(a.QSyntaxHighlighter_setDocument, G, "?setDocument@QSyntaxHighlighter@@QEAAXPEAVQTextDocument@@@Z");
        Res(a.QSyntaxHighlighter_rehighlight, G, "?rehighlight@QSyntaxHighlighter@@QEAAXXZ");
        Res(a.QSyntaxHighlighter_vtable, G, "??_7QSyntaxHighlighter@@6B@");
        Res(a.QSyntaxHighlighter_metaObject, G, "?metaObject@QSyntaxHighlighter@@UEBAPEBUQMetaObject@@XZ");
        a.core = core;
        if (!core) Log("QtLite: core Qt exports missing, extended UI disabled");
        return core;
    }
    inline bool Ready() { return A().bound && A().core; }

    // ---- QString ----------------------------------------------------------------------------------------------
    struct Str
    {
        void* d = nullptr;
        Str() = default;
        explicit Str(const char* utf8) { if (Ready()) A().QString_fromUtf8(this, utf8 ? utf8 : "", utf8 ? int(strlen(utf8)) : 0); }
        explicit Str(const std::string& utf8) : Str(utf8.c_str()) {}
        explicit Str(const std::wstring& w) { if (Ready()) A().QString_fromUtf16(this, reinterpret_cast<const unsigned short*>(w.data()), int(w.size())); }
        ~Str() { if (d) A().QString_dtor(this); }
        Str(const Str&) = delete;
        Str& operator=(const Str&) = delete;
        // QArrayData: int ref, int size, uint alloc bits, qptrdiff offset (UTF-16 data at d + offset)
        const wchar_t* Data(int& n) const
        {
            n = 0;
            if (!d) return L"";
            n = *reinterpret_cast<const int*>(static_cast<const char*>(d) + 4);
            ptrdiff_t off = *reinterpret_cast<const ptrdiff_t*>(static_cast<const char*>(d) + 16);
            return reinterpret_cast<const wchar_t*>(static_cast<const char*>(d) + off);
        }
        std::wstring Wide() const { int n; const wchar_t* w = Data(n); return n > 0 ? std::wstring(w, size_t(n)) : std::wstring(); }
        std::string Utf8() const
        {
            int n; const wchar_t* w = Data(n);
            if (n <= 0) return {};
            int bytes = WideCharToMultiByte(CP_UTF8, 0, w, n, nullptr, 0, nullptr, nullptr);
            std::string s(size_t(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, n, &s[0], bytes, nullptr, nullptr);
            return s;
        }
    };
    // Reads a QString Qt handed us by reference (signal arguments, highlightBlock's text).
    inline const wchar_t* StrData(const void* qstring, int& n) { return reinterpret_cast<const Str*>(qstring)->Data(n); }

    // ---- small value objects ------------------------------------------------------------------------------------
    struct Color
    {
        Buf b;
        Color(int r, int g, int bl, int a = 255) { if (A().QColor_ctor) A().QColor_ctor(b.p(), r, g, bl, a); }
        explicit Color(uint32_t rgb) : Color(int((rgb >> 16) & 255), int((rgb >> 8) & 255), int(rgb & 255), 255) {}
        const void* p() const { return b.p(); }
    };
    struct Variant
    {
        Buf b; bool live = false;
        Variant() = default;
        explicit Variant(int v) { if (Ready()) { A().QVariant_ctorInt(b.p(), v); live = true; } }
        explicit Variant(const Str& s) { if (Ready()) { A().QVariant_ctorString(b.p(), &s); live = true; } }
        ~Variant() { if (live) A().QVariant_dtor(b.p()); }
        Variant(const Variant&) = delete;
        Variant& operator=(const Variant&) = delete;
        const void* p() const { return b.p(); }
    };

    // An icon kept for the plugin's lifetime (QIcon is implicitly shared: copies into items are cheap).
    struct Icon
    {
        Buf b; bool live = false;
        bool Valid() const { return live; }
        const void* p() const { return b.p(); }
        void Reset() { if (live) A().QIcon_dtor(b.p()); live = false; }
        ~Icon() { /* released explicitly by Reset (Qt may be gone at process exit) */ }
    };

    // A 16x16 icon: a filled dot (shape 0), a rounded square (1) or a small folder (2) in `rgb`.
    inline bool MakeIcon(Icon& out, uint32_t rgb, int shape, int size = 16)
    {
        Api& a = A();
        if (out.live || !Ready() || !a.QPixmap_ctor || !a.QPixmap_fill || !a.QPainter_ctor || !a.QPainter_end || !a.QIcon_ctorPixmap
            || !a.QBrush_ctorColor || !a.QPainter_setBrush || !a.QPainter_setPenStyle || !a.QColor_ctor) return false;
        Buf pix; a.QPixmap_ctor(pix.p(), size, size);
        Color clear(0, 0, 0, 0); a.QPixmap_fill(pix.p(), clear.p());
        {
            Buf painter; a.QPainter_ctor(painter.p(), pix.p());
            if (a.QPainter_setRenderHint) a.QPainter_setRenderHint(painter.p(), 1 /* Antialiasing */, true);
            a.QPainter_setPenStyle(painter.p(), 0 /* NoPen */);
            Color c(rgb); Buf brush; a.QBrush_ctorColor(brush.p(), c.p(), 1 /* SolidPattern */);
            a.QPainter_setBrush(painter.p(), brush.p());
            int m = size / 4;
            if (shape == 0 && a.QPainter_drawEllipse) a.QPainter_drawEllipse(painter.p(), m, m, size - 2 * m, size - 2 * m);
            else if (shape == 1 && a.QPainter_drawRoundedRect) a.QPainter_drawRoundedRect(painter.p(), 2, 2, size - 4, size - 4, 3.0, 3.0, 0);
            else if (shape == 2 && a.QPainter_drawRoundedRect)
            {
                a.QPainter_drawRoundedRect(painter.p(), 1, 3, size / 2, 4, 1.5, 1.5, 0);          // tab
                a.QPainter_drawRoundedRect(painter.p(), 1, 5, size - 2, size - 8, 2.0, 2.0, 0);   // body
            }
            a.QPainter_end(painter.p());
            if (a.QBrush_dtor) a.QBrush_dtor(brush.p());
            if (a.QPainter_dtor) a.QPainter_dtor(painter.p());
        }
        a.QIcon_ctorPixmap(out.b.p(), pix.p());
        out.live = true;
        if (a.QPixmap_dtor) a.QPixmap_dtor(pix.p());
        return true;
    }

    // One of Radiance's own glyphs (RadianceIcons.h, drawn by GlyphRaster.h) as a size x size icon in `rgb`.
    inline bool MakeRadianceIcon(Icon& out, const char* name, uint32_t rgb, int size = 16)
    {
        Api& a = A();
        const radiance_icons::Glyph* g = glyph::Find(name);
        if (out.live || !g || !Ready() || !a.QPixmap_ctor || !a.QPixmap_convertFromImage || !a.QImage_ctor || !a.QImage_bits || !a.QImage_bytesPerLine || !a.QImage_dtor || !a.QIcon_ctorPixmap) return false;
        std::vector<uint8_t> px; glyph::Render(*g, size, rgb, px);
        // The image owns its pixels: a QPixmap built from it may share them, so they must outlive this call.
        Buf img; a.QImage_ctor(img.p(), size, size, 6 /* Format_ARGB32_Premultiplied (Qt 5: Invalid 0, Mono 1, MonoLSB 2, Indexed8 3, RGB32 4, ARGB32 5) */);
        unsigned char* bits = a.QImage_bits(img.p());
        int stride = a.QImage_bytesPerLine(img.p());
        if (!bits || stride < size * 4) { a.QImage_dtor(img.p()); return false; }
        for (int y = 0; y < size; ++y) memcpy(bits + size_t(y) * stride, px.data() + size_t(y) * size * 4, size_t(size) * 4);
        Buf pix; a.QPixmap_ctor(pix.p(), size, size);
        bool ok = a.QPixmap_convertFromImage(pix.p(), img.p(), 0);
        a.QImage_dtor(img.p());
        if (ok) { a.QIcon_ctorPixmap(out.b.p(), pix.p()); out.live = true; }
        if (a.QPixmap_dtor) a.QPixmap_dtor(pix.p());
        return ok;
    }

    // Toolbar glyphs drawn with lines (no icon files): the shapes of Radiance's toolbar icons.
    enum class Glyph { Plus, EyeOff, ExpandAll, CollapseAll, FolderPlus, File, Filter, Tiles, List, Folder, Script, Image, Audio };
    inline bool MakeGlyph(Icon& out, Glyph g, uint32_t rgb = 0xC8C8C8, int size = 16)
    {
        Api& a = A();
        if (out.live || !Ready() || !a.QPixmap_ctor || !a.QPixmap_fill || !a.QPainter_ctor || !a.QPainter_end || !a.QIcon_ctorPixmap
            || !a.QPainter_drawLine || !a.QPainter_setPenColor || !a.QPainter_setBrush || !a.QBrush_ctorColor || !a.QColor_ctor) return false;
        Buf pix; a.QPixmap_ctor(pix.p(), size, size);
        Color clear(0, 0, 0, 0); a.QPixmap_fill(pix.p(), clear.p());
        Buf painter; a.QPainter_ctor(painter.p(), pix.p());
        Color c(rgb);
        Buf none, solid;
        a.QBrush_ctorColor(none.p(), c.p(), 0 /* NoBrush */);
        a.QBrush_ctorColor(solid.p(), c.p(), 1 /* SolidPattern */);
        a.QPainter_setPenColor(painter.p(), c.p());
        a.QPainter_setBrush(painter.p(), none.p());
        // drawn on a 16-unit grid, scaled to `size` with lines as thick as one unit
        const int k = size / 16 < 1 ? 1 : size / 16;
        auto line = [&](int x1, int y1, int x2, int y2)
        {
            for (int d = 0; d < k; ++d)
            {
                bool vertical = std::abs(x2 - x1) < std::abs(y2 - y1);
                int ox = vertical ? d : 0, oy = vertical ? 0 : d;
                a.QPainter_drawLine(painter.p(), x1 * k + ox, y1 * k + oy, x2 * k + ox, y2 * k + oy);
            }
        };
        auto thick = [&](int x1, int y1, int x2, int y2) { line(x1, y1, x2, y2); if (x1 == x2) line(x1 + 1, y1, x2 + 1, y2); else line(x1, y1 + 1, x2, y2 + 1); };
        auto box = [&](int x, int y, int w, int h, bool fill)
        {
            if (fill) { for (int i = 0; i < h; ++i) line(x, y + i, x + w - 1, y + i); return; }
            line(x, y, x + w - 1, y); line(x, y + h - 1, x + w - 1, y + h - 1); line(x, y, x, y + h - 1); line(x + w - 1, y, x + w - 1, y + h - 1);
        };
        switch (g)
        {
        case Glyph::Plus: thick(7, 3, 7, 12); thick(3, 7, 12, 7); break;
        case Glyph::EyeOff:
            if (a.QPainter_drawEllipse) { a.QPainter_drawEllipse(painter.p(), 1 * k, 4 * k, 13 * k, 8 * k); a.QPainter_setBrush(painter.p(), solid.p()); a.QPainter_drawEllipse(painter.p(), 6 * k, 6 * k, 4 * k, 4 * k); }
            line(2, 14, 14, 2); line(3, 14, 14, 3); break;
        case Glyph::ExpandAll: line(3, 3, 7, 7); line(7, 7, 11, 3); line(3, 8, 7, 12); line(7, 12, 11, 8); break;
        case Glyph::CollapseAll: line(3, 7, 7, 3); line(7, 3, 11, 7); line(3, 12, 7, 8); line(7, 8, 11, 12); break;
        case Glyph::FolderPlus:
        {
            Color gold(0xD6B05C); a.QPainter_setPenColor(painter.p(), gold.p());
            box(1, 3, 6, 2, true); box(1, 5, 12, 8, true);
            a.QPainter_setPenColor(painter.p(), c.p());
            line(12, 9, 12, 15); line(9, 12, 15, 12); line(13, 9, 13, 15); line(9, 13, 15, 13);
            break;
        }
        case Glyph::File: line(3, 1, 9, 1); line(9, 1, 13, 5); line(13, 5, 13, 14); line(3, 14, 13, 14); line(3, 1, 3, 14); line(9, 1, 9, 5); line(9, 5, 13, 5); break;
        case Glyph::Filter: line(1, 2, 14, 2); line(1, 2, 6, 8); line(14, 2, 9, 8); line(6, 8, 6, 14); line(9, 8, 9, 12); line(6, 14, 9, 12); break;
        case Glyph::Tiles: box(2, 2, 5, 5, true); box(9, 2, 5, 5, true); box(2, 9, 5, 5, true); box(9, 9, 5, 5, true); break;
        case Glyph::List: for (int y : { 3, 7, 11 }) { box(1, y, 2, 2, true); line(5, y, 14, y); line(5, y + 1, 14, y + 1); } break;
        case Glyph::Folder: box(1, 3, 6, 2, true); box(1, 5, 14, 9, true); break;
        case Glyph::Script:
            line(3, 1, 9, 1); line(9, 1, 13, 5); line(13, 5, 13, 14); line(3, 14, 13, 14); line(3, 1, 3, 14); line(9, 1, 9, 5); line(9, 5, 13, 5);
            line(5, 8, 11, 8); line(5, 10, 11, 10); line(5, 12, 9, 12); break;
        case Glyph::Image: box(1, 3, 14, 11, false); line(2, 12, 6, 8); line(6, 8, 9, 11); line(9, 11, 11, 9); line(11, 9, 14, 12); box(10, 5, 2, 2, true); break;
        case Glyph::Audio:
            line(10, 2, 10, 11); line(11, 2, 11, 11); line(10, 2, 14, 4); line(10, 3, 14, 5);
            if (a.QPainter_drawEllipse) { a.QPainter_setBrush(painter.p(), solid.p()); a.QPainter_drawEllipse(painter.p(), 5 * k, 9 * k, 6 * k, 5 * k); }
            break;
        }
        a.QPainter_end(painter.p());
        if (a.QBrush_dtor) { a.QBrush_dtor(none.p()); a.QBrush_dtor(solid.p()); }
        if (a.QPainter_dtor) a.QPainter_dtor(painter.p());
        a.QIcon_ctorPixmap(out.b.p(), pix.p());
        out.live = true;
        if (a.QPixmap_dtor) a.QPixmap_dtor(pix.p());
        return true;
    }
    inline bool SetButtonIcon(void* button, const Icon& icon)
    {
        if (!button || !icon.live || !A().QAbstractButton_setIcon) return false;
        A().QAbstractButton_setIcon(button, icon.p());
        return true;
    }

    // ---- item views ---------------------------------------------------------------------------------------------
    enum { DisplayRole = 0, DecorationRole = 1, ToolTipRole = 3, FontRole = 6, ForegroundRole = 9, CheckStateRole = 10 };

    // Removes a role's value (an invalid QVariant: no data, type Invalid, the is_null bit set) so the theme applies again.
    inline void TreeItemClearRole(void* item, int column, int role)
    {
        if (!item || !A().QTreeWidgetItem_setData) return;
        Buf v;
        *reinterpret_cast<uint32_t*>(v.b + 8) = 0x80000000u;
        A().QTreeWidgetItem_setData(item, column, role, v.p());
    }
    inline int SliderValue(void* slider) { return slider && A().QAbstractSlider_value ? A().QAbstractSlider_value(slider) : 0; }
    inline void SliderSetValue(void* slider, int value) { if (slider && A().QAbstractSlider_setValue) A().QAbstractSlider_setValue(slider, value); }

    inline void TreeItemIcon(void* item, int column, const Icon& icon)
    {
        Api& a = A();
        if (!item || !icon.live || !a.QIcon_toVariant || !a.QTreeWidgetItem_setData) return;
        Buf v; a.QIcon_toVariant(icon.p(), v.p());
        a.QTreeWidgetItem_setData(item, column, DecorationRole, v.p());
        a.QVariant_dtor(v.p());
    }
    inline void TreeItemToolTip(void* item, int column, const std::string& text)
    {
        if (!item || !Ready() || !A().QTreeWidgetItem_setData) return;
        Str s(text); Variant v(s);
        A().QTreeWidgetItem_setData(item, column, ToolTipRole, v.p());
    }
    inline void TreeItemColor(void* item, int column, uint32_t rgb)
    {
        Api& a = A();
        if (!item || !a.QTreeWidgetItem_setForeground || !a.QBrush_ctorColor || !a.QColor_ctor) return;
        Color c(rgb); Buf brush; a.QBrush_ctorColor(brush.p(), c.p(), 1);
        a.QTreeWidgetItem_setForeground(item, column, brush.p());
        if (a.QBrush_dtor) a.QBrush_dtor(brush.p());
    }
    // Qt::ItemFlags: 1 selectable, 2 editable, 4 drag, 8 drop, 16 user checkable, 32 enabled, 64 tristate
    inline int TreeItemFlags(void* item) { int f = 0; if (item && A().QTreeWidgetItem_flags) A().QTreeWidgetItem_flags(item, &f); return f; }
    inline void TreeItemSetFlags(void* item, int flags) { if (item && A().QTreeWidgetItem_setFlags) A().QTreeWidgetItem_setFlags(item, flags); }
    // Qt::CheckState: 0 unchecked, 1 partially, 2 checked
    inline void TreeItemCheck(void* item, int column, int state) { if (item && A().QTreeWidgetItem_setCheckState) A().QTreeWidgetItem_setCheckState(item, column, state); }
    inline int  TreeItemCheckState(void* item, int column) { return item && A().QTreeWidgetItem_checkState ? A().QTreeWidgetItem_checkState(item, column) : -1; }
    inline bool TreeItemExpanded(void* item) { return item && A().QTreeWidgetItem_isExpanded && A().QTreeWidgetItem_isExpanded(item); }
    inline void* TreeCurrent(void* tree) { return tree && A().QTreeWidget_currentItem ? A().QTreeWidget_currentItem(tree) : nullptr; }
    inline void TreeScrollTo(void* tree, void* item) { if (tree && item && A().QTreeWidget_scrollToItem) A().QTreeWidget_scrollToItem(tree, item, 0 /* EnsureVisible */); }
    inline void* Viewport(void* scrollArea) { return scrollArea && A().QAbstractScrollArea_viewport ? A().QAbstractScrollArea_viewport(scrollArea) : nullptr; }
    inline void* VerticalScrollBar(void* scrollArea) { return scrollArea && A().QAbstractScrollArea_verticalScrollBar ? A().QAbstractScrollArea_verticalScrollBar(scrollArea) : nullptr; }

    inline QPoint CursorPos();

    inline void ListItemIcon(void* list, int row, const Icon& icon)
    {
        Api& a = A();
        if (!list || !icon.live || !a.QListWidget_item || !a.QListWidgetItem_setData || !a.QIcon_toVariant) return;
        void* item = a.QListWidget_item(list, row);
        if (!item) return;
        Buf v; a.QIcon_toVariant(icon.p(), v.p());
        a.QListWidgetItem_setData(item, DecorationRole, v.p());
        a.QVariant_dtor(v.p());
    }
    // QListView modes: tiles (IconMode, wrapping grid of `cell` px) or a plain list (ListMode).
    inline void ListViewMode(void* list, bool tiles, int iconSize, int cellW, int cellH)
    {
        Api& a = A();
        if (!list) return;
        QSize icon{ iconSize, iconSize }, grid{ tiles ? cellW : -1, tiles ? cellH : -1 };
        if (a.QListView_setViewMode) a.QListView_setViewMode(list, tiles ? 1 : 0);
        if (a.QAbstractItemView_setIconSize) a.QAbstractItemView_setIconSize(list, &icon);
        if (a.QListView_setGridSize) a.QListView_setGridSize(list, &grid);
        if (a.QListView_setResizeMode) a.QListView_setResizeMode(list, 1 /* Adjust */);
        if (a.QListView_setMovement) a.QListView_setMovement(list, 0 /* Static */);
        if (a.QListView_setWrapping) a.QListView_setWrapping(list, tiles);
        if (a.QListView_setWordWrap) a.QListView_setWordWrap(list, tiles);
        if (a.QListView_setUniformItemSizes) a.QListView_setUniformItemSizes(list, tiles);
    }
    inline void SetIconSize(void* view, int size) { QSize s{ size, size }; if (view && A().QAbstractItemView_setIconSize) A().QAbstractItemView_setIconSize(view, &s); }

    // A QFont kept alive by its owner (QWidget::setFont copies it).
    struct Font
    {
        Buf b; bool live = false;
        bool Make(const char* family, int pointSize, int weight = 50)
        {
            if (live || !Ready() || !A().QFont_ctor) return false;
            Str f(family);
            A().QFont_ctor(b.p(), &f, pointSize, weight, false);
            live = true;
            return true;
        }
        void SetPointSize(int pt) { if (live && A().QFont_setPointSize) A().QFont_setPointSize(b.p(), pt); }
        void Apply(void* widget) const { if (live && widget && A().QWidget_setFont) A().QWidget_setFont(widget, b.p()); }
        int AverageCharWidth() const
        {
            if (!live || !A().QFontMetrics_ctor || !A().QFontMetrics_averageCharWidth) return 7;
            Buf m; A().QFontMetrics_ctor(m.p(), b.p());
            int w = A().QFontMetrics_averageCharWidth(m.p());
            if (A().QFontMetrics_dtor) A().QFontMetrics_dtor(m.p());
            return w > 0 ? w : 7;
        }
        void Reset() { if (live && A().QFont_dtor) A().QFont_dtor(b.p()); live = false; }
    };

    // ---- QPlainTextEdit ------------------------------------------------------------------------------------------
    inline std::wstring PlainText(void* edit)
    {
        if (!edit || !Ready() || !A().QPlainTextEdit_toPlainText) return {};
        Str s; A().QPlainTextEdit_toPlainText(edit, &s);
        return s.Wide();
    }
    inline void SetPlainText(void* edit, const std::wstring& text)
    {
        if (!edit || !Ready() || !A().QPlainTextEdit_setPlainText) return;
        Str s(text); A().QPlainTextEdit_setPlainText(edit, &s);
    }
    inline void* Document(void* edit) { return edit && A().QPlainTextEdit_document ? A().QPlainTextEdit_document(edit) : nullptr; }
    inline bool DocModified(void* edit) { void* d = Document(edit); return d && A().QTextDocument_isModified && A().QTextDocument_isModified(d); }
    inline void SetDocModified(void* edit, bool on) { void* d = Document(edit); if (d && A().QTextDocument_setModified) A().QTextDocument_setModified(d, on); }
    // The caret: position, block (0-based line), column, selection.
    struct Caret { int position = 0, line = 0, column = 0, selStart = 0, selEnd = 0; bool ok = false; };
    inline Caret CaretOf(void* edit)
    {
        Caret c; Api& a = A();
        if (!edit || !a.QPlainTextEdit_textCursor || !a.QTextCursor_dtor || !a.QTextCursor_position) return c;
        Buf cur; a.QPlainTextEdit_textCursor(edit, cur.p());
        c.position = a.QTextCursor_position(cur.p());
        if (a.QTextCursor_blockNumber) c.line = a.QTextCursor_blockNumber(cur.p());
        if (a.QTextCursor_positionInBlock) c.column = a.QTextCursor_positionInBlock(cur.p());
        if (a.QTextCursor_selectionStart) c.selStart = a.QTextCursor_selectionStart(cur.p());
        if (a.QTextCursor_selectionEnd) c.selEnd = a.QTextCursor_selectionEnd(cur.p());
        a.QTextCursor_dtor(cur.p());
        c.ok = true;
        return c;
    }
    // Selects [anchor, position] (equal = just moves the caret) and shows it.
    inline void SetCaret(void* edit, int anchor, int position, bool center)
    {
        Api& a = A();
        if (!edit || !a.QPlainTextEdit_textCursor || !a.QTextCursor_setPosition || !a.QPlainTextEdit_setTextCursor) return;
        Buf cur; a.QPlainTextEdit_textCursor(edit, cur.p());
        a.QTextCursor_setPosition(cur.p(), anchor, 0 /* MoveAnchor */);
        if (position != anchor) a.QTextCursor_setPosition(cur.p(), position, 1 /* KeepAnchor */);
        a.QPlainTextEdit_setTextCursor(edit, cur.p());
        a.QTextCursor_dtor(cur.p());
        if (center && a.QPlainTextEdit_centerCursor) a.QPlainTextEdit_centerCursor(edit);
    }
    // Applies edits (offset, remove, insert) as one undo step, in the order given.
    struct TextEdit { int offset; int remove; std::wstring insert; };
    inline bool ApplyEdits(void* edit, const std::vector<TextEdit>& edits)
    {
        Api& a = A();
        if (!edit || edits.empty() || !a.QPlainTextEdit_textCursor || !a.QTextCursor_setPosition || !a.QTextCursor_insertText
            || !a.QTextCursor_removeSelectedText || !a.QTextCursor_beginEditBlock || !a.QTextCursor_endEditBlock) return false;
        Buf cur; a.QPlainTextEdit_textCursor(edit, cur.p());
        a.QTextCursor_beginEditBlock(cur.p());
        for (const TextEdit& e : edits)
        {
            a.QTextCursor_setPosition(cur.p(), e.offset, 0);
            if (e.remove > 0) { a.QTextCursor_setPosition(cur.p(), e.offset + e.remove, 1); a.QTextCursor_removeSelectedText(cur.p()); }
            if (!e.insert.empty()) { Str s(e.insert); a.QTextCursor_insertText(cur.p(), &s); }
        }
        a.QTextCursor_endEditBlock(cur.p());
        a.QTextCursor_dtor(cur.p());
        return true;
    }
    inline void InsertText(void* edit, const std::wstring& text) { if (edit && Ready() && A().QPlainTextEdit_insertPlainText) { Str s(text); A().QPlainTextEdit_insertPlainText(edit, &s); } }
    // QTextDocument::FindFlags: 1 backward, 2 case sensitively, 4 whole words
    inline bool Find(void* edit, const std::wstring& text, int flags)
    {
        if (!edit || text.empty() || !Ready() || !A().QPlainTextEdit_find) return false;
        Str s(text);
        return A().QPlainTextEdit_find(edit, &s, flags);
    }
    inline std::wstring SelectedText(void* edit)
    {
        Api& a = A();
        if (!edit || !a.QPlainTextEdit_textCursor || !a.QTextCursor_selectedText) return {};
        Buf cur; a.QPlainTextEdit_textCursor(edit, cur.p());
        Str s; a.QTextCursor_selectedText(cur.p(), &s);
        a.QTextCursor_dtor(cur.p());
        std::wstring w = s.Wide();
        for (wchar_t& c : w) if (c == 0x2029) c = L'\n';   // QTextCursor's paragraph separator
        return w;
    }
    // The caret's rectangle in global (screen) coordinates, for popups.
    inline QPoint CaretGlobalBottom(void* edit)
    {
        Api& a = A();
        QRect r{};
        void* vp = Viewport(edit);
        if (!edit || !vp || !a.QPlainTextEdit_cursorRect || !a.QWidget_mapToGlobal) return CursorPos();
        a.QPlainTextEdit_cursorRect(edit, &r);
        QPoint local{ r.x1, r.y2 + 2 }, global{};
        a.QWidget_mapToGlobal(vp, &global, &local);
        return global;
    }

    inline QPoint CursorPos()
    {
        QPoint p{};
        if (A().QCursor_pos) A().QCursor_pos(&p);
        else { POINT w{}; GetCursorPos(&w); p = { w.x, w.y }; }
        return p;
    }
    // The cursor in a widget's own coordinates.
    inline QPoint CursorIn(void* widget)
    {
        QPoint g = CursorPos();
        QPoint zero{ 0, 0 }, origin{};
        if (widget && A().QWidget_mapToGlobal) { A().QWidget_mapToGlobal(widget, &origin, &zero); return { g.x - origin.x, g.y - origin.y }; }
        return g;
    }
    // The tree row under the mouse cursor (the tree's viewport holds the rows).
    inline void* TreeItemUnderCursor(void* tree)
    {
        void* vp = Viewport(tree);
        if (!vp || !A().QTreeWidget_itemAt) return nullptr;
        QPoint p = CursorIn(vp);
        return A().QTreeWidget_itemAt(tree, p.x, p.y);
    }

    // Text the host API cannot carry: it builds QStrings as Latin-1, so anything beyond ASCII is set again here as UTF-8.
    inline bool IsAscii(const std::string& s) { for (unsigned char c : s) if (c >= 0x80) return false; return true; }
    inline void TreeItemText(void* item, int column, const std::string& utf8)
    {
        if (!item || IsAscii(utf8) || !Ready() || !A().QTreeWidgetItem_setText) return;
        Str s(utf8); A().QTreeWidgetItem_setText(item, column, &s);
    }
    inline void ListItemText(void* list, int row, const std::string& utf8)
    {
        if (!list || IsAscii(utf8) || !Ready() || !A().QListWidget_item || !A().QListWidgetItem_setText) return;
        void* item = A().QListWidget_item(list, row);
        if (item) { Str s(utf8); A().QListWidgetItem_setText(item, &s); }
    }
    // Rich-text labels through the host: characters beyond ASCII as HTML character references.
    inline std::string HtmlAscii(const std::string& utf8)
    {
        if (IsAscii(utf8)) return utf8;
        int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
        std::wstring w(size_t(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), &w[0], n);
        std::string out;
        for (size_t i = 0; i < w.size(); ++i)
        {
            unsigned cp = w[i];
            if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < w.size()) { cp = 0x10000 + ((cp - 0xD800) << 10) + (unsigned(w[i + 1]) - 0xDC00); ++i; }
            if (cp < 0x80) out += char(cp); else out += "&#" + std::to_string(cp) + ";";
        }
        return out;
    }
    // A label (or other widget) that may be narrower than its text: it is clipped instead of widening the pane.
    inline void IgnoreWidth(void* widget) { if (widget && A().QWidget_setSizePolicy) A().QWidget_setSizePolicy(widget, 13 /* Ignored */, 5 /* Preferred */); }

    // The selected tree items. QList<T*> is one pointer to QListData::Data { ref, alloc, begin, end, array[] }.
    inline std::vector<void*> TreeSelectedItems(void* tree)
    {
        std::vector<void*> out;
        Api& a = A();
        if (!tree || !a.QTreeWidget_selectedItems) return out;
        void* d = nullptr;
        a.QTreeWidget_selectedItems(tree, &d);
        if (!d) return out;
        int* h = static_cast<int*>(d);
        void** items = reinterpret_cast<void**>(static_cast<char*>(d) + 16);
        for (int i = h[2]; i < h[3]; ++i) out.push_back(items[i]);
        if (h[0] > 0 && InterlockedDecrement(reinterpret_cast<volatile LONG*>(h)) == 0 && a.QListData_dispose) a.QListData_dispose(d);
        return out;
    }
    inline void TreeItemSelect(void* item, bool on) { if (item && A().QTreeWidgetItem_setSelected) A().QTreeWidgetItem_setSelected(item, on); }
    inline void ClearSelection(void* view) { if (view && A().QAbstractItemView_clearSelection) A().QAbstractItemView_clearSelection(view); }

    // Whether a list row is under the mouse cursor.
    inline bool ListItemUnderCursor(void* list)
    {
        void* vp = Viewport(list);
        if (!vp || !A().QListWidget_itemAt) return false;
        QPoint p = CursorIn(vp);
        return A().QListWidget_itemAt(list, p.x, p.y) != nullptr;
    }

    inline void SetUpdates(void* widget, bool on) { if (widget && A().QWidget_setUpdatesEnabled) A().QWidget_setUpdatesEnabled(widget, on); }
    inline void SetStyleSheet(void* widget, const char* css) { if (widget && Ready() && A().QWidget_setStyleSheet) { Str s(css); A().QWidget_setStyleSheet(widget, &s); } }
    inline bool Inherits(const void* obj, const char* cls) { return obj && A().QObject_inherits && A().QObject_inherits(obj, cls); }
    inline void* FocusWidget() { return A().QApplication_focusWidget ? A().QApplication_focusWidget() : nullptr; }
    // A text field has the keyboard (typing there must not count as a Radiant key press).
    inline bool TextInputFocused()
    {
        void* f = FocusWidget();
        return f && (Inherits(f, "QLineEdit") || Inherits(f, "QTextEdit") || Inherits(f, "QPlainTextEdit") || Inherits(f, "QAbstractSpinBox"));
    }

    // ---- event filters ------------------------------------------------------------------------------------------
    // fn(user, watched, type, event) returns true to swallow. The QEvent fields used: type at +16, the accepted bit
    // (bit 2 of the ushort at +18). A QKeyEvent has its modifiers at +24 and the key code at +40.
    using FilterFn = bool (*)(void* user, void* watched, int type, void* event);
    struct FilterObj { unsigned char qobject[32]; FilterFn fn; void* user; std::vector<void*>* watched; };
    struct FilterState { void* vt[16] = {}; void** original = nullptr; std::vector<FilterObj*> filters; };
    inline FilterState& Filters() { static FilterState s; return s; }
    inline bool __fastcall FilterThunk(void* self, void* watched, void* ev)
    {
        auto* fo = static_cast<FilterObj*>(self);
        int type = *reinterpret_cast<unsigned short*>(static_cast<char*>(ev) + 16);
        return fo->fn && fo->fn(fo->user, watched, type, ev);
    }
    inline void* MakeFilter(FilterFn fn, void* user)
    {
        if (!Ready()) return nullptr;
        auto* fo = static_cast<FilterObj*>(A().opNew(sizeof(FilterObj)));
        if (!fo) return nullptr;
        memset(fo, 0, sizeof *fo);
        A().QObject_ctor(fo, nullptr);
        FilterState& s = Filters();
        if (!s.original)
        {
            // QObject vtable: 0 metaObject, 1 qt_metacast, 2 qt_metacall, 3 deleting dtor, 4 event, 5 eventFilter, 6..10
            s.original = *reinterpret_cast<void***>(fo);
            memcpy(s.vt, s.original, sizeof s.vt);
            s.vt[5] = reinterpret_cast<void*>(&FilterThunk);
        }
        *reinterpret_cast<void***>(fo) = s.vt;
        fo->fn = fn; fo->user = user;
        s.filters.push_back(fo);
        return fo;
    }
    inline void Install(void* obj, void* filter) { if (obj && filter) A().QObject_installEventFilter(obj, filter); }
    inline void Uninstall(void* obj, void* filter) { if (obj && filter) A().QObject_removeEventFilter(obj, filter); }
    inline int  EventType(void* ev) { return *reinterpret_cast<unsigned short*>(static_cast<char*>(ev) + 16); }
    inline void AcceptEvent(void* ev) { *reinterpret_cast<unsigned short*>(static_cast<char*>(ev) + 18) |= 4; }
    inline void IgnoreEvent(void* ev) { *reinterpret_cast<unsigned short*>(static_cast<char*>(ev) + 18) &= static_cast<unsigned short>(~4u); }
    // The Qt widget behind a native window (Radiant's main window from host->MainWindow), or null.
    inline void* WidgetForHwnd(void* hwnd)
    {
        static void* (*find)(uintptr_t) = nullptr;
        if (!find) find = reinterpret_cast<void* (*)(uintptr_t)>(GetProcAddress(GetModuleHandleW(L"Qt5Widgets64r.dll"), "?find@QWidget@@SAPEAV1@_K@Z"));
        return find && hwnd ? find(reinterpret_cast<uintptr_t>(hwnd)) : nullptr;
    }
    inline int  KeyCode(void* keyEvent) { return *reinterpret_cast<int*>(static_cast<char*>(keyEvent) + 40); }
    inline int  KeyMods(void* keyEvent) { return *reinterpret_cast<int*>(static_cast<char*>(keyEvent) + 24); }
    enum { ShiftMod = 0x02000000, CtrlMod = 0x04000000, AltMod = 0x08000000 };
    enum { EvMouseButtonPress = 2, EvMouseButtonRelease = 3, EvMouseButtonDblClick = 4, EvKeyPress = 6, EvClose = 19, EvShortcutOverride = 51,
           EvDragMove = 61, EvDrop = 63, EvContextMenu = 82 };
    enum { Key_Escape = 0x01000000, Key_Tab = 0x01000001, Key_Backtab = 0x01000002, Key_Backspace = 0x01000003, Key_Return = 0x01000004,
           Key_Enter = 0x01000005, Key_Delete = 0x01000007, Key_F2 = 0x01000031, Key_F3 = 0x01000032, Key_F5 = 0x01000034 };

    // ---- signals ------------------------------------------------------------------------------------------------
    // QtPrivate::QSlotObjectBase: { QAtomicInt ref, ImplFn impl } with our payload behind it. impl(which, ...): 0 Destroy, 1 Call.
    using SlotFn = void (*)(void* user, void** args);
    struct SlotObj { int ref; int pad; void (*impl)(int, SlotObj*, void*, void**, bool*); SlotFn fn; void* user; void* connection; };
    inline std::vector<SlotObj*>& Slots() { static std::vector<SlotObj*> v; return v; }
    inline void SlotImpl(int which, SlotObj* self, void*, void** args, bool* ret)
    {
        if (which == 1) { if (self->fn) self->fn(self->user, args); }
        else if (which == 2) { if (ret) *ret = false; }
        else if (which == 0) { self->fn = nullptr; }     // Qt let go of it; the memory stays ours
    }
    // Connects `signal` (address of the signal's member function, its class's staticMetaObject) of `sender` to fn.
    inline bool Connect(void* sender, void* signal, const void* meta, SlotFn fn, void* user)
    {
        Api& a = A();
        if (!Ready() || !sender || !signal || !meta || !a.QObject_connectImpl || !a.QObject_disconnect || !a.Connection_dtor) return false;
        auto* so = static_cast<SlotObj*>(a.opNew(sizeof(SlotObj)));
        if (!so) return false;
        memset(so, 0, sizeof *so);
        so->ref = 1; so->impl = &SlotImpl; so->fn = fn; so->user = user;
        void* signalPtr[3] = { signal, nullptr, nullptr };   // member-function pointer: code address + zero adjustment
        a.QObject_connectImpl(&so->connection, sender, signalPtr, sender, nullptr, so, 0, nullptr, meta);
        if (!so->connection) { Log("QtLite: connect failed"); return false; }
        Slots().push_back(so);
        return true;
    }

    // ---- menus and dialogs --------------------------------------------------------------------------------------
    // A popup menu built, shown at the cursor and destroyed in one call: add entries, then Exec returns the chosen index.
    struct Menu
    {
        void* menu = nullptr;
        std::vector<void*> actions;
        explicit Menu(void* parent)
        {
            if (!Ready() || !A().QMenu_ctor || !A().QMenu_addAction || !A().QMenu_exec) return;
            menu = A().opNew(256);
            if (menu) { memset(menu, 0, 256); A().QMenu_ctor(menu, parent); }
        }
        ~Menu() { if (menu) A().QObject_deleteLater(menu); }
        bool Ok() const { return menu != nullptr; }
        int Add(const std::string& text, bool enabled = true, int checked = -1)
        {
            if (!menu) return -1;
            Str s(text);
            void* act = A().QMenu_addAction(menu, &s);
            if (act && checked >= 0 && A().QAction_setCheckable) { A().QAction_setCheckable(act, true); A().QAction_setChecked(act, checked != 0); }
            if (act && !enabled && A().QAction_setEnabled) A().QAction_setEnabled(act, false);
            actions.push_back(act);
            return int(actions.size()) - 1;
        }
        void Separator() { if (menu && A().QMenu_addSeparator) A().QMenu_addSeparator(menu); }
        int Exec()
        {
            if (!menu || actions.empty()) return -1;
            QPoint at = CursorPos();
            void* chosen = A().QMenu_exec(menu, &at, nullptr);
            for (size_t i = 0; i < actions.size(); ++i) if (actions[i] && actions[i] == chosen) return int(i);
            return -1;
        }
        int ExecAt(QPoint at)
        {
            if (!menu || actions.empty()) return -1;
            void* chosen = A().QMenu_exec(menu, &at, nullptr);
            for (size_t i = 0; i < actions.size(); ++i) if (actions[i] && actions[i] == chosen) return int(i);
            return -1;
        }
    };

    // Modal text prompt. Returns false when cancelled (or unavailable).
    inline bool AskText(void* parent, const std::string& title, const std::string& label, const std::string& initial, std::string& out)
    {
        if (!Ready() || !A().QInputDialog_getText) return false;
        Str t(title), l(label), i(initial), result;
        bool ok = false;
        A().QInputDialog_getText(&result, parent, &t, &l, 0, &i, &ok, 0, 0);
        if (!ok) return false;
        out = result.Utf8();
        return true;
    }
    inline bool AskInt(void* parent, const std::string& title, const std::string& label, int value, int minValue, int maxValue, int& out)
    {
        if (!Ready() || !A().QInputDialog_getInt) return false;
        Str t(title), l(label);
        bool ok = false;
        int v = A().QInputDialog_getInt(parent, &t, &l, value, minValue, maxValue, 1, &ok, 0);
        if (!ok) return false;
        out = v;
        return true;
    }
    // QMessageBox::StandardButton values
    enum { BtnOk = 0x400, BtnSave = 0x800, BtnYes = 0x4000, BtnNo = 0x10000, BtnCancel = 0x400000, BtnDiscard = 0x800000 };
    inline int Question(void* parent, const std::string& title, const std::string& text, int buttons, int defaultButton)
    {
        if (!Ready() || !A().QMessageBox_question) return MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_YESNO) == IDYES ? BtnYes : BtnNo;
        Str t(title), x(text);
        return A().QMessageBox_question(parent, &t, &x, buttons, defaultButton);
    }
    inline void Warn(void* parent, const std::string& title, const std::string& text)
    {
        if (!Ready() || !A().QMessageBox_warning) { MessageBoxA(nullptr, text.c_str(), title.c_str(), MB_OK | MB_ICONWARNING); return; }
        Str t(title), x(text);
        A().QMessageBox_warning(parent, &t, &x, BtnOk, BtnOk);
    }

    // ---- syntax highlighting ------------------------------------------------------------------------------------
    // A QSyntaxHighlighter without moc: Qt's own class with highlightBlock (vtable slot 11, after QObject's 11) pointed
    // at a C function. The vtable is checked against the exported one before anything is patched.
    using HighlightFn = void (*)(void* highlighter, void* user, const wchar_t* text, int length);
    struct HighlighterObj { unsigned char qobject[16]; HighlightFn fn; void* user; };
    struct HighlightState { void* vt[16] = {}; bool checked = false, usable = false; std::vector<HighlighterObj*> live; };
    inline HighlightState& Highlighters() { static HighlightState s; return s; }
    inline void __fastcall HighlightThunk(void* self, const void* qstring)
    {
        auto* h = static_cast<HighlighterObj*>(self);
        int n = 0; const wchar_t* text = StrData(qstring, n);
        if (h->fn) h->fn(self, h->user, text, n);
    }
    inline bool HighlightingAvailable()
    {
        HighlightState& s = Highlighters();
        if (s.checked) return s.usable;
        s.checked = true;
        Api& a = A();
        if (!Ready() || !a.QSyntaxHighlighter_ctor || !a.QSyntaxHighlighter_setFormat || !a.QSyntaxHighlighter_vtable || !a.QSyntaxHighlighter_metaObject
            || !a.QSyntaxHighlighter_setDocument || !a.QSyntaxHighlighter_previousBlockState || !a.QSyntaxHighlighter_setCurrentBlockState || !a.QColor_ctor) return false;
        void** vt = a.QSyntaxHighlighter_vtable;
        // slot 0 must be QSyntaxHighlighter::metaObject and slot 11 the pure virtual highlightBlock: Qt5Gui's import stub
        // (jmp [rip+disp]) to msvcr110's _purecall
        if (vt[0] != a.QSyntaxHighlighter_metaObject) { Log("QtLite: QSyntaxHighlighter vtable layout not recognised"); return false; }
        void* slot = vt[11];
        const unsigned char* code = static_cast<const unsigned char*>(slot);
        if (code[0] == 0xFF && code[1] == 0x25)
        {
            int32_t disp; memcpy(&disp, code + 2, 4);
            slot = *reinterpret_cast<void* const*>(code + 6 + disp);
        }
        void* purecall = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"msvcr110.dll"), "_purecall"));
        if (!purecall || slot != purecall) { Log("QtLite: QSyntaxHighlighter slot 11 is not the pure virtual"); return false; }
        memcpy(s.vt, vt, 12 * sizeof(void*));
        s.vt[11] = reinterpret_cast<void*>(&HighlightThunk);
        s.usable = true;
        return true;
    }
    // Attaches a highlighter to a QTextDocument (it becomes the document's child).
    inline void* AttachHighlighter(void* document, HighlightFn fn, void* user)
    {
        if (!document || !HighlightingAvailable()) return nullptr;
        auto* h = static_cast<HighlighterObj*>(A().opNew(64));
        if (!h) return nullptr;
        memset(h, 0, 64);
        h->fn = fn; h->user = user;
        A().QSyntaxHighlighter_ctor(h, document);
        *reinterpret_cast<void***>(h) = Highlighters().vt;
        Highlighters().live.push_back(h);
        if (A().QSyntaxHighlighter_rehighlight) A().QSyntaxHighlighter_rehighlight(h);
        return h;
    }
    inline void SetFormat(void* highlighter, int start, int count, uint32_t rgb)
    {
        if (count <= 0) return;
        Color c(rgb);
        A().QSyntaxHighlighter_setFormat(highlighter, start, count, c.p());
    }
    inline int  PreviousBlockState(void* highlighter) { return A().QSyntaxHighlighter_previousBlockState(highlighter); }
    inline void SetBlockState(void* highlighter, int state) { A().QSyntaxHighlighter_setCurrentBlockState(highlighter, state); }
    inline void Rehighlight(void* highlighter) { if (highlighter && A().QSyntaxHighlighter_rehighlight) A().QSyntaxHighlighter_rehighlight(highlighter); }
    // Detaches and destroys a highlighter now (before its document goes, or on unload).
    inline void DestroyHighlighter(void* highlighter)
    {
        auto& live = Highlighters().live;
        auto it = std::find(live.begin(), live.end(), static_cast<HighlighterObj*>(highlighter));
        if (it == live.end()) return;
        live.erase(it);
        auto* h = static_cast<HighlighterObj*>(highlighter);
        h->fn = nullptr;
        A().QSyntaxHighlighter_setDocument(h, nullptr);
        void** original = A().QSyntaxHighlighter_vtable;
        *reinterpret_cast<void***>(h) = original;
        reinterpret_cast<void (*)(void*, unsigned)>(original[3])(h, 1);   // scalar deleting destructor
    }

    // ---- teardown -----------------------------------------------------------------------------------------------
    // Called from OnUnload: nothing Qt keeps may point into this DLL once the host frees it.
    inline void Shutdown()
    {
        if (!A().bound) return;
        for (SlotObj* so : Slots())
        {
            so->fn = nullptr;
            if (so->connection) { A().QObject_disconnect(&so->connection); A().Connection_dtor(&so->connection); so->connection = nullptr; }
        }
        Slots().clear();     // Qt may still hold a dead slot object; its impl is never called after disconnect
        auto live = Highlighters().live;
        for (HighlighterObj* h : live) DestroyHighlighter(h);
        FilterState& fs = Filters();
        for (FilterObj* fo : fs.filters)
        {
            fo->fn = nullptr;
            *reinterpret_cast<void***>(fo) = fs.original;                                   // Qt's own QObject vtable again
            reinterpret_cast<void (*)(void*, unsigned)>(fs.original[3])(fo, 1);             // destroy now (watchers drop it)
        }
        fs.filters.clear();
    }
}
