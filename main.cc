#include "lockless_ring.h"
#include <string>
#include <thread>
#include <string.h>
#include <csignal>
#include <unistd.h>

struct Person {
    int    age;
    int    height;
    int    weight;
    std::string name;
};

bool prod_flag = true;
bool cons_flag = true;

static uint64_t prodpkt1 = 0;
static uint64_t prodpkt2 = 0;
static uint64_t conspkt1 = 0;
static uint64_t conspkt2 = 0;

void ProducerThreadFunc(struct LLR<Person>* llr, uint64_t* prodpkt, Person* p, int ring_size) {
    int i            = 0;
    int enqueue_size = 7;
    while (prod_flag) {
        int ret = ring_enqueue(llr, &p[i % ring_size], enqueue_size);
        if (ret == -1) {
            // "ring full"
            continue;
        }
        (*prodpkt) += enqueue_size;
        ++i;
    }
}

void ConsumerThreadFunc(struct LLR<Person>* llr, uint64_t* conspkt, int thread_id) {
    Person* p    = nullptr;
    int     dequeue_size = 5;
    while (cons_flag) {
        p = ring_dequeue(llr, dequeue_size);
        if (p == nullptr) {
            continue;
        }
        (*conspkt) += dequeue_size;
    }
}

// Signal Handler for SIGINT
void signalHandler(int signum) {
    prod_flag = false;
    sleep(1);
    cons_flag = false;
}

void LKTest() {
    // Register signal and signal handler
    signal(SIGINT, signalHandler);

    auto   llr = InitStructLLR<Person>(10);  // size: 2 ^ 10 = 1024 type: Person
    Person p[1024];
    memset(p, 0x00, sizeof(p));
    for (int i = 0; i < 1024; ++i) {
        p[i].age = i * 3;
    }

    std::thread prod1(ProducerThreadFunc, llr, &prodpkt1, p, 1024);
    std::thread prod2(ProducerThreadFunc, llr, &prodpkt2, p, 1024);
    std::thread cons1(ConsumerThreadFunc, llr, &conspkt1, 1);
    std::thread cons2(ConsumerThreadFunc, llr, &conspkt2, 2);

    // sleep(1);
    if (prod1.joinable()) {
        prod1.join();
    }
    if (prod2.joinable()) {
        prod2.join();
    }
    if (cons1.joinable()) {
        cons1.join();
    }
    if (cons2.joinable()) {
        cons2.join();
    }

    std::cout << std::endl;
    std::cout << "prod1: " << prodpkt1 << " prod2: " << prodpkt2 << " sum: " << prodpkt1 + prodpkt2 << std::endl;
    std::cout << "cons1: " << conspkt1 << " cons2: " << conspkt2 << " sum: " << conspkt1 + conspkt2 << std::endl;
    std::cout << "cons_head: " << llr->llr_cons_head_ << " cons_tail: " << llr->llr_cons_tail_
              << " prod_head: " << llr->llr_prod_head_ << " prod_tail: " << llr->llr_prod_tail_ << std::endl;
    std::cout << "ring count: " << ring_count(llr) << std::endl;
}

int main() { LKTest(); }