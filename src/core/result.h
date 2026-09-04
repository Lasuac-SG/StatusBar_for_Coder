#pragma once

#include <QString>

#include <stdexcept>
#include <utility>
#include <variant>

namespace Core {

template <typename T>
class Result final {
public:
    [[nodiscard]] static Result success(T value)
    {
        return Result(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static Result failure(QString error)
    {
        return Result(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept { return valueOrError_.index() == 0; }

    [[nodiscard]] T& value() & { return std::get<0>(valueOrError_); }
    [[nodiscard]] const T& value() const& { return std::get<0>(valueOrError_); }
    [[nodiscard]] T value() && { return std::get<0>(std::move(valueOrError_)); }
    [[nodiscard]] T value() const&& { return std::get<0>(valueOrError_); }

    [[nodiscard]] const QString& error() const& noexcept
    {
        static const QString noError;
        return hasValue() ? noError : std::get<1>(valueOrError_);
    }

    [[nodiscard]] QString error() && noexcept
    {
        return hasValue() ? QString{} : std::get<1>(std::move(valueOrError_));
    }

    [[nodiscard]] QString error() const&& noexcept
    {
        return hasValue() ? QString{} : std::get<1>(valueOrError_);
    }

private:
    Result(std::in_place_index_t<0>, T value)
        : valueOrError_(std::in_place_index<0>, std::move(value))
    {
    }

    Result(std::in_place_index_t<1>, QString error)
        : valueOrError_(std::in_place_index<1>, std::move(error))
    {
    }

    std::variant<T, QString> valueOrError_;
};

template <>
class Result<void> final {
public:
    [[nodiscard]] static Result success() { return Result(true, {}); }

    [[nodiscard]] static Result failure(QString error)
    {
        return Result(false, std::move(error));
    }

    [[nodiscard]] bool hasValue() const noexcept { return success_; }

    void value() const
    {
        if (!success_) {
            throw std::logic_error("Result has no value");
        }
    }

    [[nodiscard]] const QString& error() const& noexcept { return error_; }
    [[nodiscard]] QString error() && noexcept { return std::move(error_); }
    [[nodiscard]] QString error() const&& noexcept { return error_; }

private:
    Result(bool success, QString error)
        : success_(success)
        , error_(std::move(error))
    {
    }

    bool success_{};
    QString error_;
};

} // namespace Core
