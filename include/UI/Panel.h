#ifndef PANEL_H
#define PANEL_H

class Panel {
  public:
    Panel() : m_y(0), m_x(0), m_w(0), m_h(0) {}
    virtual ~Panel() = default;

    void setDimensions(int y, int x, int w, int h) {
        m_y = y;
        m_x = x;
        m_w = w;
        m_h = h;
    }

  protected:
    int m_y, m_x, m_w, m_h;
};

#endif // PANEL_H
