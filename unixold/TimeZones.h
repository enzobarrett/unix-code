class TimeZones {
  public:
    static double getNext();
    static double getCurrent();

  private:
    static int index;
    const static double zones[];
    const static int length;
};
