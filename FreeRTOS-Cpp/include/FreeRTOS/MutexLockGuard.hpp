#ifndef FREERTOS_LOCK_GUARD_HPP
#define FREERTOS_LOCK_GUARD_HPP

#include <FreeRTOS/Mutex.hpp>
#include <cassert>

namespace FreeRTOS {

// ============================================================================
// Теги для перегрузки конструкторов
// ============================================================================

struct adopt_lock_t { explicit adopt_lock_t() = default; };
inline constexpr adopt_lock_t adopt_lock{};

struct defer_lock_t { explicit defer_lock_t() = default; };
inline constexpr defer_lock_t defer_lock{};

// ============================================================================
// lock_guard с поддержкой таймаута
// ============================================================================

template <typename MutexType>
class lock_guard {
private:
    MutexType& m_mutex;
    bool m_owned;

public:
    using mutex_type = MutexType;

    /// @brief Конструктор 1: Блокирует до успеха (как std::lock_guard)
    explicit lock_guard(mutex_type& m) : m_mutex(m), m_owned(true) {
        [[maybe_unused]] const bool acquired = m_mutex.lock();
        m_owned = acquired;
        // не асертиим.
        assert(acquired && "FreeRTOS::lock_guard: failed to acquire mutex");
    }

    /// @brief Конструктор 2: Блокирует с таймаутом
    /// @return Если блокировка не удалась, m_owned = false, и unlock() НЕ вызовется
    lock_guard(mutex_type& m, TickType_t timeout) : m_mutex(m), m_owned(false) {
        m_owned = m_mutex.lock(timeout);
        // Без assert — это нормальная ситуация при таймауте
    }

    /// @brief Конструктор 3: Принятие владения (мьютекс уже заблокирован)
    lock_guard(mutex_type& m, adopt_lock_t) : m_mutex(m), m_owned(true) {
        // Предполагается, что вызывающий код уже успешно вызвал m.lock()
    }

    /// @brief Конструктор 4: Отложенная блокировка (не блокирует вообще)
    lock_guard(mutex_type& m, defer_lock_t) : m_mutex(m), m_owned(false) {
        // Не блокирует. Пользователь должен вызвать lock() вручную.
    }

    ~lock_guard() {
        if (m_owned) {
            [[maybe_unused]] const bool released = m_mutex.unlock();
            assert(released && "FreeRTOS::lock_guard: failed to release mutex");
        }
    }

    // === Дополнительные методы для работы с флагом владения ===
    
    /// @brief Проверить, владеет ли guard мьютексом
    bool owns_lock() const noexcept { return m_owned; }
    explicit operator bool() const noexcept { return m_owned; }

    /// @brief Ручная блокировка (для defer_lock)
    bool lock() {
        if (!m_owned) {
            m_owned = m_mutex.lock();
        }
        return m_owned;
    }

    /// @brief Ручная блокировка с таймаутом
    bool lock(TickType_t timeout) {
        if (!m_owned) {
            m_owned = m_mutex.lock(timeout);
        }
        return m_owned;
    }

    /// @brief Ручная разблокировка
    bool unlock() {
        if (m_owned) {
            m_owned = false;
            return m_mutex.unlock();
        }
        return false;
    }

    /// @brief Доступ к базовому мьютексу
    mutex_type* mutex() const noexcept { return &m_mutex; }

    // Запрет копирования и перемещения
    lock_guard(const lock_guard&) = delete;
    lock_guard& operator=(const lock_guard&) = delete;
    lock_guard(lock_guard&&) = delete;
    lock_guard& operator=(lock_guard&&) = delete;
};

// CTAD guides (C++17)
#if __cplusplus >= 201703L
    template <typename Mutex>
    lock_guard(Mutex&) -> lock_guard<Mutex>;

    template <typename Mutex>
    lock_guard(Mutex&, TickType_t) -> lock_guard<Mutex>;

    template <typename Mutex>
    lock_guard(Mutex&, adopt_lock_t) -> lock_guard<Mutex>;

    template <typename Mutex>
    lock_guard(Mutex&, defer_lock_t) -> lock_guard<Mutex>;
#endif

} // namespace FreeRTOS

#endif // FREERTOS_LOCK_GUARD_HPP