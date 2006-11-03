///////////////////////////////////////////////////////////////////////
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
#ifndef WENDY_UIWIDGET_H
#define WENDY_UIWIDGET_H
///////////////////////////////////////////////////////////////////////

namespace wendy
{
  namespace UI
  {
  
///////////////////////////////////////////////////////////////////////

using namespace moira;

///////////////////////////////////////////////////////////////////////

/*! @defgroup ui User interface API
 *
 *  These classes provide a simple 2D graphical user interface (GUI). The
 *  rendering is layered on top of the OpenGL renderer and using the input
 *  functionality of the context singleton, so they environment must be set
 *  up before the user interface can be rendered.
 *
 *  The user interface classes make heavy use of signals, and its design is in
 *  many ways similar to the gtkmm library. One notable difference is that,
 *  since we are working on top of OpenGL, usually together with 3D rendering,
 *  we need to re-render the entire interface each frame.
 */

///////////////////////////////////////////////////////////////////////

/*! @brief Base class for interface widgets.
 *  @ingroup ui
 *
 *  This is the base class for all interface widgets, including windows.
 *  It also translates and dispatches user input events, with no client
 *  setup required.
 *  
 *  @remarks Yes, it's big. Get over it.
 */
class Widget : public Managed<Widget>,
	       public Trackable
{
public:
  typedef std::list<Widget*> List;
  /*! Constructor.
   *  @param[in] name The desired name of the widget, or the empty string to
   *  automatically generate a name.
   *
   *  @remarks The widget name is not the same as the label of a button or
   *  title of a window.
   */
  Widget(const String& name = "");
  /*! Destructor.
   */
  ~Widget(void);
  void addChild(Widget& child);
  void removeChild(Widget& child);
  /*! Searches for a widget at the specified point.
   *  @param[in] point The point at which to search.
   *  @return The widget at the specified point, or @c NULL if no matching
   *  widget was found.
   *
   *  @remarks The point is in parent coordinates. If this is a top-level
   *  widget, it is in global coordinates.
   */
  Widget* findByPoint(const Vector2& point);
  /*! Transforms the specified point from global into local (client
   *  area) coordinates.
   *  @param[in] globalPoint The global coordinate point to transform.
   *  @return The corresponding local coordinate point.
   */
  Vector2 transformToLocal(const Vector2& globalPoint) const;
  /*! Transforms the specified point from local (client area) into
   *  global coordinates.
   *  @param[in] localPoint The local coordinate point to transform.
   *  @return The corresponding global coordinate point.
   */
  Vector2 transformToGlobal(const Vector2& localPoint) const;
  /*! Enables this widget, allowing it to become active and receive input
   *  events.
   */
  void enable(void);
  /*! Disables this widget, disallowing activation and reception of input
   *  events.
   */
  void disable(void);
  /*! Makes this widget visible.
   */
  void show(void);
  /*! Makes this widget hidden.
   */
  void hide(void);
  /*! Makes this the active widget.
   *
   *  @remarks This may fail if the widget is disabled.
   */
  void activate(void);
  /*! @return @c true if this widget is enabled, otherwise @c false.
   */
  bool isEnabled(void) const;
  /*! @return @c true if this widget is visible, otherwise @c false.
   */
  bool isVisible(void) const;
  /*! @return @c true if this is the active widget, otherwise @c false.
   */
  bool isActive(void) const;
  bool isUnderCursor(void) const;
  bool isBeingDragged(void) const;
  Widget* getParent(void) const;
  const List& getChildren(void) const;
  WidgetState getState(void) const;
  /*! @return The area of this widget, in parent coordinates.
   */
  const Rectangle& getArea(void) const;
  /*! @return The area of this widget, in global coordinates.
   */
  const Rectangle& getGlobalArea(void) const;
  /*! Sets the area of this widget.
   *  @param[in] newArea The desired area, in parent coordinates.
   */
  void setArea(const Rectangle& newArea);
  /*! Sets the size of this widget.
   *  @param[in] newSize The desired size, in parent coordinates.
   *
   *  @remarks This is a helper method for Widget::setArea.
   */
  void setSize(const Vector2& newSize);
  /*! Sets the position of this widget.
   *  @param[in] newPosition The desired position, in parent coordinates.
   *
   *  @remarks This is a helper method for Widget::setArea.
   */
  void setPosition(const Vector2& newPosition);
  SignalProxy1<void, Widget&> getDestroySignal(void);
  SignalProxy2<void, Widget&, const Rectangle&> getChangeAreaSignal(void);
  SignalProxy2<void, Widget&, bool> getChangeFocusSignal(void);
  SignalProxy3<void, Widget&, GL::Key, bool> getKeyPressSignal(void);
  SignalProxy2<void, Widget&, wchar_t> getCharInputSignal(void);
  SignalProxy2<void, Widget&, const Vector2&> getCursorMoveSignal(void);
  SignalProxy4<void, Widget&, const Vector2&, unsigned int, bool> getButtonClickSignal(void);
  SignalProxy1<void, Widget&> getCursorEnterSignal(void);
  SignalProxy1<void, Widget&> getCursorLeaveSignal(void);
  SignalProxy2<void, Widget&, const Vector2&> getDragBeginSignal(void);
  SignalProxy2<void, Widget&, const Vector2&> getDragMoveSignal(void);
  SignalProxy2<void, Widget&, const Vector2&> getDragEndSignal(void);
  /*! @return The active widget, or @c NULL if no widget is active.
   */
  static Widget* getActive(void);
  /*! Renders all root level widgets.
   */
  static void renderRoots(void);
protected:
  /*! Calls Widget::render for all children of this widget.
   */
  virtual void render(void) const;
  virtual void addedChild(Widget& child);
  virtual void removedChild(Widget& child);
  virtual void addedToParent(Widget& parent);
  virtual void removedFromParent(Widget& parent);
private:
  static void onKeyPress(GL::Key key, bool pressed);
  static void onCharInput(wchar_t character);
  static void onCursorMove(const Vector2& position);
  static void onButtonClick(unsigned int button, bool clicked);
  Signal1<void, Widget&> destroySignal;
  Signal2<void, Widget&, const Rectangle&> changeAreaSignal;
  Signal2<void, Widget&, bool> changeFocusSignal;
  Signal3<void, Widget&, GL::Key, bool> keyPressSignal;
  Signal2<void, Widget&, wchar_t> charInputSignal;
  Signal2<void, Widget&, const Vector2&> cursorMoveSignal;
  Signal4<void, Widget&, const Vector2&, unsigned int, bool> buttonClickSignal;
  Signal1<void, Widget&> cursorEnterSignal;
  Signal1<void, Widget&> cursorLeaveSignal;
  Signal2<void, Widget&, const Vector2&> dragBeginSignal;
  Signal2<void, Widget&, const Vector2&> dragMoveSignal;
  Signal2<void, Widget&, const Vector2&> dragEndSignal;
  Widget* parent;
  List children;
  bool enabled;
  bool visible;
  bool underCursor;
  bool beingDragged;
  Rectangle area;
  mutable Rectangle globalArea;
  static bool dragging;
  static List roots;
  static Widget* activeWidget;
  static Widget* draggedWidget;
};

///////////////////////////////////////////////////////////////////////

  } /*namespace UI*/
} /*namespace wendy*/

///////////////////////////////////////////////////////////////////////
#endif /*WENDY_UIWIDGET_H*/
///////////////////////////////////////////////////////////////////////
