//////////////////////////////////////////////////////////////////////
// Wendy user interface library
// Copyright (c) 2006 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any
// damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any
// purpose, including commercial applications, and to alter it and
// redistribute it freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you
//     must not claim that you wrote the original software. If you use
//     this software in a product, an acknowledgment in the product
//     documentation would be appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and
//     must not be misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source
//     distribution.
//
///////////////////////////////////////////////////////////////////////

#include <wendy/Config.h>

#include <wendy/UIDrawer.h>
#include <wendy/UILayer.h>
#include <wendy/UIWidget.h>
#include <wendy/UIScroller.h>
#include <wendy/UIItem.h>
#include <wendy/UIList.h>

#include <algorithm>

///////////////////////////////////////////////////////////////////////

namespace wendy
{
  namespace UI
  {

///////////////////////////////////////////////////////////////////////

namespace
{

struct ItemComparator
{
  inline bool operator () (const Item* x, const Item* y)
  {
    return *x < *y;
  }
};

}

///////////////////////////////////////////////////////////////////////

List::List(Layer& layer):
  Widget(layer),
  offset(0),
  maxOffset(0),
  selection(NO_ITEM)
{
  getAreaChangedSignal().connect(*this, &List::onAreaChanged);
  getButtonClickedSignal().connect(*this, &List::onButtonClicked);
  getKeyPressedSignal().connect(*this, &List::onKeyPressed);
  getWheelTurnedSignal().connect(*this, &List::onWheelTurned);

  scroller = new Scroller(layer, VERTICAL);
  scroller->setValueRange(0.f, 1.f);
  scroller->setPercentage(1.f);
  scroller->getValueChangedSignal().connect(*this, &List::onValueChanged);
  addChild(*scroller);

  onAreaChanged(*this);
}

List::~List()
{
  destroyItems();
}

void List::addItem(Item& item)
{
  if (std::find(items.begin(), items.end(), &item) != items.end())
    return;

  items.push_back(&item);
  updateScroller();
}

void List::createItem(const char* value, ItemID ID)
{
  Item* item = new Item(getLayer(), value, ID);
  addItem(*item);
}

Item* List::findItem(const char* value)
{
  for (ItemList::const_iterator i = items.begin();  i != items.end();  i++)
  {
    if ((*i)->asString() == value)
      return *i;
  }

  return NULL;
}

const Item* List::findItem(const char* value) const
{
  for (ItemList::const_iterator i = items.begin();  i != items.end();  i++)
  {
    if ((*i)->asString() == value)
      return *i;
  }

  return NULL;
}

void List::destroyItem(Item& item)
{
  ItemList::iterator i = std::find(items.begin(), items.end(), &item);
  assert(i != items.end());

  if (selection == i - items.begin())
    setSelection(NO_ITEM, false);

  delete *i;
  items.erase(i);
  updateScroller();
}

void List::destroyItems()
{
  while (!items.empty())
  {
    delete items.back();
    items.pop_back();
  }

  setSelection(NO_ITEM, false);
  updateScroller();
}

void List::sortItems()
{
  ItemComparator comparator;
  std::sort(items.begin(), items.end(), comparator);

  updateScroller();
}

bool List::isItemVisible(const Item& item) const
{
  unsigned int index = 0;

  while (index < items.size() && items[index] != &item)
    index++;

  if (index == items.size())
    return false;

  if (index < offset)
    return false;

  float height = items[index]->getHeight();

  for (unsigned int i = offset;  i < index;  i++)
  {
    height += items[i]->getHeight();
    if (height >= getHeight())
      return false;
  }

  return true;
}

unsigned int List::getOffset() const
{
  return offset;
}

void List::setOffset(unsigned int newOffset)
{
  offset = min(newOffset, maxOffset);
  scroller->setValue(float(offset));
}

unsigned int List::getSelection() const
{
  return selection;
}

void List::setSelection(unsigned int newSelection)
{
  setSelection(newSelection, false);
}

Item* List::getSelectedItem() const
{
  if (selection == NO_ITEM)
    return NULL;

  assert(selection < items.size());
  return items[selection];
}

void List::setSelectedItem(Item& newItem)
{
  ItemList::const_iterator i = std::find(items.begin(), items.end(), &newItem);
  assert(i != items.end());
  selection = i - items.begin();
}

unsigned int List::getItemCount() const
{
  return (unsigned int) items.size();
}

Item* List::getItem(unsigned int index)
{
  if (index < items.size())
  {
    ItemList::iterator i = items.begin();
    std::advance(i, index);
    return *i;
  }

  return NULL;
}

const Item* List::getItem(unsigned int index) const
{
  if (index < items.size())
  {
    ItemList::const_iterator i = items.begin();
    std::advance(i, index);
    return *i;
  }

  return NULL;
}

SignalProxy2<void, List&, unsigned int> List::getItemSelectedSignal()
{
  return itemSelectedSignal;
}

void List::draw() const
{
  const Rect& area = getGlobalArea();

  Drawer& drawer = getLayer().getDrawer();
  if (drawer.pushClipArea(area))
  {
    drawer.drawWell(area, getState());

    float start = area.size.y;

    for (unsigned int i = offset;  i < items.size();  i++)
    {
      const Item& item = *items[i];

      float height = item.getHeight();
      if (height + start < 0.f)
        break;

      Rect itemArea = area;
      itemArea.position.y += start - height;
      itemArea.size.y = height;

      item.draw(itemArea, i == selection ? STATE_SELECTED : STATE_NORMAL);

      start -= height;
    }

    Widget::draw();

    drawer.popClipArea();
  }
}

void List::onAreaChanged(Widget& widget)
{
  const float width = scroller->getWidth();

  scroller->setArea(Rect(getWidth() - width, 0.f, width, getHeight()));
  updateScroller();
}

void List::onButtonClicked(Widget& widget,
                           const vec2& position,
                           input::Button button,
                           bool clicked)
{
  if (!clicked)
    return;

  vec2 localPosition = transformToLocal(position);

  const float height = getHeight();
  float itemTop = height;

  for (unsigned int i = offset;  i < items.size();  i++)
  {
    const float itemHeight = items[i]->getHeight();
    if (itemTop - itemHeight < 0.f)
      break;

    if (itemTop - itemHeight <= localPosition.y)
    {
      setSelection(i, true);
      return;
    }

    itemTop -= itemHeight;
  }
}

void List::onKeyPressed(Widget& widget, input::Key key, bool pressed)
{
  if (!pressed)
    return;

  switch (key)
  {
    case input::KEY_UP:
    {
      if (selection == NO_ITEM)
        setSelection(items.size() - 1, true);
      else if (selection > 0)
        setSelection(selection - 1, true);
      break;
    }

    case input::KEY_DOWN:
    {
      if (selection == NO_ITEM)
        setSelection(0, true);
      else
        setSelection(selection + 1, true);
      break;
    }

    case input::KEY_HOME:
    {
      setSelection(0, true);
      break;
    }

    case input::KEY_END:
    {
      if (!items.empty())
        setSelection(items.size() - 1, true);
      break;
    }
  }
}

void List::onWheelTurned(Widget& widget, int wheelOffset)
{
  if (items.empty())
    return;

  if (wheelOffset + (int) offset < 0)
    return;

  setOffset(offset + wheelOffset);
}

void List::onValueChanged(Scroller& scroller)
{
  setOffset((unsigned int) scroller.getValue());
}

void List::updateScroller()
{
  maxOffset = 0;

  float totalItemHeight = 0.f;

  for (ItemList::const_iterator i = items.begin();  i != items.end();  i++)
    totalItemHeight += (*i)->getHeight();

  float visibleItemHeight = 0.f;

  for (ItemList::const_reverse_iterator i = items.rbegin();  i != items.rend();  i++)
  {
    visibleItemHeight += (*i)->getHeight();
    if (visibleItemHeight > getHeight())
    {
      maxOffset = items.rend() - i;
      break;
    }
  }

  if (maxOffset)
  {
    scroller->show();
    scroller->setValueRange(0.f, maxOffset);
    scroller->setPercentage(getHeight() / totalItemHeight);
  }
  else
    scroller->hide();

  setOffset(offset);
}

void List::setSelection(unsigned int newSelection, bool notify)
{
  if (newSelection == NO_ITEM)
  {
    selection = newSelection;
    invalidate();
  }
  else
  {
    selection = min(newSelection, (unsigned int) items.size() - 1);

    if (isItemVisible(*items[selection]))
      invalidate();
    else
      setOffset(selection);
  }

  if (notify)
    itemSelectedSignal(*this, selection);
}

///////////////////////////////////////////////////////////////////////

  } /*namespace UI*/
} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
