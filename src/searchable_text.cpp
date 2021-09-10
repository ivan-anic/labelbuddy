#include <QAbstractSlider>
#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLineEdit>
#include <QList>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QScrollBar>
#include <QStyle>
#include <QTextDocument>
#include <QVBoxLayout>
#include <QWidget>

#include "searchable_text.h"

namespace labelbuddy {

SearchableText::SearchableText(QWidget* parent) : QWidget(parent) {
  QVBoxLayout* top_layout = new QVBoxLayout();
  setLayout(top_layout);

  text_edit = new QPlainTextEdit();
  top_layout->addWidget(text_edit);
  text_edit->installEventFilter(this);
  auto palette = text_edit->palette();
  palette.setColor(QPalette::Inactive, QPalette::Highlight,
                   palette.color(QPalette::Active, QPalette::Highlight));
  palette.setColor(QPalette::Inactive, QPalette::HighlightedText,
                   palette.color(QPalette::Active, QPalette::HighlightedText));
  text_edit->setPalette(palette);

  QHBoxLayout* search_bar_layout = new QHBoxLayout();
  top_layout->addLayout(search_bar_layout);

  search_box = new QLineEdit();
  search_bar_layout->addWidget(search_box);
  search_box->installEventFilter(this);
  search_box->setPlaceholderText("Search in document ( / or Ctrl+F )");
  find_prev_button = new QPushButton();
  search_bar_layout->addWidget(find_prev_button);
  find_next_button = new QPushButton();
  search_bar_layout->addWidget(find_next_button);
  find_prev_button->setIcon(
      QIcon::fromTheme("go-up", QIcon(":data/icons/go-up.png")));
  find_next_button->setIcon(
      QIcon::fromTheme("go-down", QIcon(":data/icons/go-down.png")));

  QAction* search_action = new QAction(this);
  search_action->setShortcuts(
      QList<QKeySequence>{QKeySequence::Find, QKeySequence::FindNext});
  QObject::connect(search_action, &QAction::triggered, this,
                   &SearchableText::search_forward);

  QObject::connect(find_next_button, &QPushButton::clicked, this,
                   &SearchableText::search_forward);
  QObject::connect(find_prev_button, &QPushButton::clicked, this,
                   &SearchableText::search_backward);
  QObject::connect(search_box, &QLineEdit::textChanged, this,
                   &SearchableText::update_search_button_states);

  QObject::connect(text_edit, &QPlainTextEdit::selectionChanged, this,
                   &SearchableText::set_cursor_position);
  update_search_button_states();
}

void SearchableText::fill(const QString& content) {
  text_edit->setPlainText(content);
  text_edit->setProperty("readOnly", true);
  this->setFocus();
}

void SearchableText::update_search_button_states() {
  auto has_pattern = search_box->text() != QString();
  find_next_button->setEnabled(has_pattern);
  find_prev_button->setEnabled(has_pattern);
}

void SearchableText::search_forward() { search(); }
void SearchableText::search_backward() { search(QTextDocument::FindBackward); }

void SearchableText::search(QTextDocument::FindFlags flags) {
  this->setFocus();
  auto pattern = search_box->text();
  if (pattern.isEmpty()) {
    return;
  }
  auto document = text_edit->document();
  current_search_flags = flags;
  auto top_left = text_edit->cursorForPosition(text_edit->rect().topLeft());
  auto bottom_right =
      text_edit->cursorForPosition(text_edit->rect().bottomRight());
  if (last_match < top_left || last_match >= bottom_right) {
    last_match =
        (flags & QTextDocument::FindBackward) ? bottom_right : top_left;
  }
  auto found = document->find(pattern, last_match, flags);
  if (found.isNull()) {
    auto new_cursor = text_edit->textCursor();
    if (flags & QTextDocument::FindBackward) {
      new_cursor.movePosition(QTextCursor::End);
    } else {
      new_cursor.movePosition(QTextCursor::Start);
    }
    found = document->find(pattern, new_cursor, flags);
  }
  if (!found.isNull()) {
    last_match = found;
    text_edit->setTextCursor(last_match);
  }
}

void SearchableText::set_cursor_position() {
  last_match = text_edit->textCursor();
}

void SearchableText::swap_pos_anchor(QTextCursor& cursor) const {
  auto pos = cursor.position();
  auto anchor = cursor.anchor();
  cursor.setPosition(pos);
  cursor.setPosition(anchor, QTextCursor::KeepAnchor);
}

void SearchableText::extend_selection(QTextCursor::MoveOperation move_op,
                                      SelectionSide side) {
  auto cursor = text_edit->textCursor();
  bool swapped{};
  auto anchor = cursor.anchor();
  auto pos = cursor.position();
  if (anchor == pos) {
    if (side == SelectionSide::Right &&
        (move_op == QTextCursor::PreviousWord ||
         move_op == QTextCursor::PreviousCharacter)) {
      return;
    }
    if (side == SelectionSide::Left &&
        (move_op == QTextCursor::NextWord ||
         move_op == QTextCursor::NextCharacter)) {
      return;
    }
  }
  if ((anchor > pos && side == SelectionSide::Right) ||
      (anchor < pos && side == SelectionSide::Left)) {
    swap_pos_anchor(cursor);
    swapped = true;
  }
  cursor.movePosition(move_op, QTextCursor::KeepAnchor);
  if (swapped) {
    swap_pos_anchor(cursor);
  }
  text_edit->setTextCursor(cursor);
}

bool SearchableText::eventFilter(QObject* object, QEvent* event) {
  if (event->type() == QEvent::KeyPress) {
    auto key_event = static_cast<QKeyEvent*>(event);
    if (object == search_box) {
      if (((key_event->modifiers() & Qt::ControlModifier) &&
           (nav_keys.contains(key_event->key())))) {
        handle_nav_event(key_event);
        return true;
      }
    }
    for (auto seq : selection_sequences) {
      if (key_event->matches(seq)) {
        handle_nav_event(key_event);
        return true;
      }
    }
  }
  return QWidget::eventFilter(object, event);
}

void SearchableText::cycle_cursor_height() {
  auto top = text_edit->cursorRect().top();
  for (int i = 0; i < 3; ++i) {
    cycle_cursor_height_once();
    if (text_edit->cursorRect().top() != top) {
      return;
    }
  }
}

void SearchableText::cycle_cursor_height_once() {
  text_edit->ensureCursorVisible();

  auto pos = text_edit->textCursor().position();
  auto bottom = text_edit->rect().bottom();
  auto top = text_edit->rect().top();
  auto center = (bottom + top) / 2;

  CursorHeight target_height;
  if (pos != last_cursor_pos) {
    target_height = CursorHeight::Center;
    last_cursor_pos = pos;
  } else {
    target_height = static_cast<CursorHeight>(
        (static_cast<int>(last_cursor_height) + 1) % 3);
  }
  switch (target_height) {
  case CursorHeight::Center:
    scroll_to_position(center, TopOrBottom::Bottom);
    break;
  case CursorHeight::Top:
    scroll_to_position(top, TopOrBottom::Top);
    break;
  case CursorHeight::Bottom:
    scroll_to_position(bottom, TopOrBottom::Bottom);
    break;
  }
  last_cursor_height = target_height;
}

bool SearchableText::scroll_to_position(int target, TopOrBottom cursor_side) {
  auto crect = text_edit->cursorRect();
  auto line_height = (crect.bottom() - crect.top());
  auto pos = get_cursor_pos(cursor_side);
  auto prev_pos = pos;
  auto initial_pos = pos;
  auto scroll_bar = text_edit->verticalScrollBar();
  if (pos <= target - line_height) {
    // scroll up ie move cursor down on the screen
    do {
      prev_pos = pos;
      scroll_bar->triggerAction(QAbstractSlider::SliderSingleStepSub);
      pos = get_cursor_pos(cursor_side);
    } while ((pos != prev_pos) && (pos <= target - line_height));
    return pos != initial_pos;
  }

  if (pos >= target + line_height) {
    // scroll down ie move cursor up on the screen
    do {
      prev_pos = pos;
      scroll_bar->triggerAction(QAbstractSlider::SliderSingleStepAdd);
      pos = get_cursor_pos(cursor_side);
    } while ((pos != prev_pos) && (pos >= target + line_height));
    return pos != initial_pos;
  }
  return false;
}

int SearchableText::get_cursor_pos(TopOrBottom top_bottom) {
  return top_bottom == TopOrBottom::Top ? text_edit->cursorRect().top()
                                        : text_edit->cursorRect().bottom();
}

void SearchableText::handle_nav_event(QKeyEvent* event) {
  if (((event->key() == Qt::Key_J) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      ((event->key() == Qt::Key_N) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      (event->matches(QKeySequence::MoveToNextLine))) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderSingleStepAdd);
    return;
  }
  if (((event->key() == Qt::Key_K) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      ((event->key() == Qt::Key_P) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      (event->matches(QKeySequence::MoveToPreviousLine))) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderSingleStepSub);
    return;
  }
  if (((event->key() == Qt::Key_D) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      event->matches(QKeySequence::MoveToNextPage)) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderPageStepAdd);
    return;
  }
  if (((event->key() == Qt::Key_U) &&
       (event->modifiers() & Qt::ControlModifier)) ||
      event->matches(QKeySequence::MoveToPreviousPage)) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderPageStepSub);
    return;
  }
  if (event->key() == Qt::Key_End ||
      event->matches(QKeySequence::MoveToEndOfDocument)) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderToMaximum);
    return;
  }
  if (event->key() == Qt::Key_Home ||
      event->matches(QKeySequence::MoveToStartOfDocument)) {
    text_edit->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderToMinimum);
    return;
  }
  if ((event->key() == Qt::Key_L) &&
      (event->modifiers() & Qt::ControlModifier)) {
    cycle_cursor_height();
    return;
  }
  if ((event->key() == Qt::Key_BracketRight) &&
      (event->modifiers() & Qt::ControlModifier)) {
    extend_selection(QTextCursor::NextCharacter, SelectionSide::Right);
    return;
  }
  if ((event->key() == Qt::Key_BracketLeft) &&
      (event->modifiers() & Qt::ControlModifier)) {
    extend_selection(QTextCursor::PreviousCharacter, SelectionSide::Right);
    return;
  }
  if ((event->key() == Qt::Key_BraceRight) &&
      (event->modifiers() & Qt::ControlModifier)) {
    extend_selection(QTextCursor::NextCharacter, SelectionSide::Left);
    return;
  }
  if ((event->key() == Qt::Key_BraceLeft) &&
      (event->modifiers() & Qt::ControlModifier)) {
    extend_selection(QTextCursor::PreviousCharacter, SelectionSide::Left);
    return;
  }

  if ((event->key() == Qt::Key_BracketRight)) {
    extend_selection(QTextCursor::NextWord, SelectionSide::Right);
    return;
  }
  if ((event->key() == Qt::Key_BracketLeft)) {
    extend_selection(QTextCursor::PreviousWord, SelectionSide::Right);
    return;
  }
  if ((event->key() == Qt::Key_BraceRight)) {
    extend_selection(QTextCursor::NextWord, SelectionSide::Left);
    return;
  }
  if ((event->key() == Qt::Key_BraceLeft)) {
    extend_selection(QTextCursor::PreviousWord, SelectionSide::Left);
    return;
  }
  if (event->matches(QKeySequence::SelectNextChar)) {
    extend_selection(QTextCursor::NextCharacter, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::SelectPreviousChar)) {
    extend_selection(QTextCursor::PreviousCharacter, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::SelectNextWord)) {
    extend_selection(QTextCursor::NextWord, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::SelectPreviousWord)) {
    extend_selection(QTextCursor::PreviousWord, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::SelectNextLine)) {
    extend_selection(QTextCursor::Down, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::SelectPreviousLine)) {
    extend_selection(QTextCursor::Up, SelectionSide::Cursor);
    return;
  }
  if (event->matches(QKeySequence::Paste)) {
    extend_selection(QTextCursor::End, SelectionSide::Cursor);
    return;
  }
  QWidget::keyPressEvent(event);
}

void SearchableText::keyPressEvent(QKeyEvent* event) {
  if (event->matches(QKeySequence::Find) || event->key() == Qt::Key_Slash) {
    search_box->setFocus();
    search_box->selectAll();
    return;
  }
  if (event->matches(QKeySequence::InsertParagraphSeparator)) {
    search_forward();
    return;
  }
  if (event->matches(QKeySequence::InsertLineSeparator)) {
    search_backward();
    return;
  }
  handle_nav_event(event);
}

QTextCursor SearchableText::textCursor() const {
  return text_edit->textCursor();
}
QPlainTextEdit* SearchableText::get_text_edit() { return text_edit; }
QLineEdit* SearchableText::get_search_box() { return search_box; }

QList<int> SearchableText::current_selection() const {
  QTextCursor cursor = text_edit->textCursor();
  return QList<int>{cursor.selectionStart(), cursor.selectionEnd()};
}
} // namespace labelbuddy
