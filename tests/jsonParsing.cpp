#include <string>
#include <unordered_map>
#include <variant>
#include <iostream>
#include <vector>
#include <utility>

struct JSONValue
{
    using Object = std::unordered_map<std::string, JSONValue>;
    using Array = std::vector<JSONValue>;

    std::variant<
        std::nullptr_t,
        bool,
        int,
        double,
        std::string,
        Object,
        Array>
        value;

    JSONValue() : value(nullptr) {}
    JSONValue(bool v) : value(v) {}
    JSONValue(int v) : value(v) {}
    JSONValue(double v) : value(v) {}
    JSONValue(const char *v) : value(std::string(v)) {}
    JSONValue(const std::string &v) : value(v) {}
    JSONValue(Object v) : value(std::move(v)) {}
    JSONValue(Array v) : value(std::move(v)) {}

    JSONValue &operator[](const std::string &key)
    {
        if (!std::holds_alternative<Object>(value))
            value = Object{};

        return std::get<Object>(value)[key];
    }

    JSONValue &operator[](size_t index)
    {
        if (!std::holds_alternative<Array>(value))
            value = Array{};

        auto &array = std::get<Array>(value);

        if (index >= array.size())
            array.resize(index + 1);

        return array[index];
    }

    void push_back(JSONValue v)
    {
        if (!std::holds_alternative<Array>(value))
            value = Array{};

        std::get<Array>(value).push_back(std::move(v));
    }

    void print(int indent = 0) const
    {
        std::visit([indent](const auto &data)
                   {
                       using T = std::decay_t<decltype(data)>;

                       if constexpr (std::is_same_v<T, std::nullptr_t>)
                       {
                           std::cout << "null";
                       }
                       else if constexpr (std::is_same_v<T, bool>)
                       {
                           std::cout << (data ? "true" : "false");
                       }
                       else if constexpr (std::is_same_v<T, int>)
                       {
                           std::cout << data;
                       }
                       else if constexpr (std::is_same_v<T, double>)
                       {
                           std::cout << data;
                       }
                       else if constexpr (std::is_same_v<T, std::string>)
                       {
                           std::cout << '"' << data << '"';
                       }
                       else if constexpr (std::is_same_v<T, Object>)
                       {
                           std::cout << "{\n";

                           bool first = true;

                           for (const auto &[key, value] : data)
                           {
                               if (!first)
                                   std::cout << ",\n";

                               first = false;

                               std::cout << std::string(indent + 4, ' ')
                                         << '"' << key << "\": ";

                               value.print(indent + 4);
                           }

                           std::cout << '\n'
                                     << std::string(indent, ' ')
                                     << '}';
                       }
                       else if constexpr (std::is_same_v<T, Array>)
                       {
                           std::cout << "[\n";

                           for (size_t i = 0; i < data.size(); ++i)
                           {
                               std::cout << std::string(indent + 4, ' ');

                               data[i].print(indent + 4);

                               if (i + 1 < data.size())
                                   std::cout << ',';

                               std::cout << '\n';
                           }

                           std::cout << std::string(indent, ' ')
                                     << ']';
                       } },
                   value);
    }
};

int main()
{

    JSONValue root = JSONValue::Object{
        {"people",
         JSONValue::Array{
             JSONValue::Object{
                 {"name", "Ram"},
                 {"age", 32},
                 {"email", "ram@gmail.com"}},
             JSONValue::Object{
                 {"name", "Aadi"},
                 {"age", 22},
                 {"email", "aadi@gmail.com"}}}}};

    root.print();

    // JSONValue root;

    // root["people"].push_back(
    //     JSONValue::Object{
    //         {"name", "Ram"},
    //         {"age", 32},
    //         {"email", "ram@gmail.com"}});

    // root["people"].push_back(
    //     JSONValue::Object{
    //         {"name", "Aadi"},
    //         {"age", 22},
    //         {"email", "aadi@gmail.com"}});

    return 0;
}