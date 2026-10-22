#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

class ConcurrentList {
public:
    struct Node {
        explicit Node(std::int64_t v) : value(v) {}
        const std::int64_t value;
        Node* prev = nullptr;
        Node* next = nullptr;
        std::mutex m;
    };

    explicit ConcurrentList(const std::vector<std::int64_t>& initial) {
        head_ = new Node(0);
        tail_ = new Node(0);
        head_->next = tail_;
        tail_->prev = head_;
        Node* last = head_;
        for (std::int64_t v : initial) {
            Node* n = new Node(v);
            n->prev = last;
            n->next = tail_;
            last->next = n;
            tail_->prev = n;
            last = n;
        }
    }

    ~ConcurrentList() {
        Node* n = head_;
        while (n) {
            Node* next = n->next;
            delete n;
            n = next;
        }
    }

    ConcurrentList(const ConcurrentList&) = delete;
    ConcurrentList& operator=(const ConcurrentList&) = delete;

    Node* first() { return moveNext(head_); }
    Node* last() { return movePrev(tail_); }

    Node* moveNext(Node* n) {
        Node* r;
        {
            std::lock_guard<std::mutex> lk(n->m);
            r = n->next;
        }
        return r == tail_ ? nullptr : r;
    }

    Node* movePrev(Node* n) {
        Node* r;
        {
            std::lock_guard<std::mutex> lk(n->m);
            r = n->prev;
        }
        return r == head_ ? nullptr : r;
    }

    Node* insertAfter(Node* n, std::int64_t value) {
        Node* created = new Node(value);
        std::lock_guard<std::mutex> left(n->m);
        Node* right = n->next;
        std::lock_guard<std::mutex> rightLock(right->m);
        link(n, created, right);
        return created;
    }

    Node* insertBefore(Node* n, std::int64_t value) {
        Node* created = new Node(value);
        for (;;) {
            Node* left;
            {
                std::lock_guard<std::mutex> lk(n->m);
                left = n->prev;
            }
            std::lock_guard<std::mutex> leftLock(left->m);
            std::lock_guard<std::mutex> rightLock(n->m);
            if (left->next == n && n->prev == left) {
                link(left, created, n);
                return created;
            }
        }
    }

    std::vector<std::int64_t> forward() const {
        std::vector<std::int64_t> out;
        for (Node* n = head_->next; n != tail_; n = n->next) out.push_back(n->value);
        return out;
    }

    std::vector<std::int64_t> backward() const {
        std::vector<std::int64_t> out;
        for (Node* n = tail_->prev; n != head_; n = n->prev) out.push_back(n->value);
        return out;
    }

    bool validate(std::string& error) const {
        std::size_t count = 0;
        for (Node* n = head_; n != tail_; n = n->next) {
            if (!n->next) { error = "null next pointer"; return false; }
            if (n->next->prev != n) { error = "next->prev does not point back"; return false; }
            if (++count > 1'000'000'000) { error = "cycle in next pointers"; return false; }
        }
        std::size_t back = 0;
        for (Node* n = tail_; n != head_; n = n->prev) {
            if (!n->prev) { error = "null prev pointer"; return false; }
            ++back;
        }
        if (back != count) { error = "forward and backward lengths differ"; return false; }
        return true;
    }

private:
    static void link(Node* left, Node* created, Node* right) {
        created->prev = left;
        created->next = right;
        left->next = created;
        right->prev = created;
    }

    Node* head_;
    Node* tail_;
};
