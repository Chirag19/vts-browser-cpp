#ifndef DATATHREAD_H_wefvwehjzg
#define DATATHREAD_H_wefvwehjzg

#include <thread>
#include <vts-browser/fetcher.hpp>

class GLFWwindow;

namespace vts
{
class Map;
}

class DataThread
{
public:
    DataThread(GLFWwindow *shared, double *timing);
    ~DataThread();

    void run();

    std::shared_ptr<vts::Fetcher> fetcher;
    std::thread thr;
    vts::Map *map;
    GLFWwindow *window;
    double *timing;
    volatile bool stop;
};

#endif
