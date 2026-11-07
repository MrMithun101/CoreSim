#include <algorithm>
#include <cctype>
#include <cmath>
#include <coresim/studio/SceneFile.hpp>
#include <cstdio>
#include <fstream>
#include <picojson.h>
#include <stdexcept>
namespace coresim {
namespace {
using V = picojson::value;
using A = picojson::array;
using O = picojson::object;
constexpr std::size_t max_bytes = 2 * 1024 * 1024;
V vec(glm::vec3 v) { return V(A{V(double(v.x)), V(double(v.y)), V(double(v.z))}); }
template <class T> const T& get(const V& v) {
    if (!v.is<T>())
        throw std::invalid_argument("Missing or invalid scene field");
    return v.get<T>();
}
float scalar(const V& v) {
    auto x = get<double>(v);
    if (!std::isfinite(x) || std::abs(x) > 1e6)
        throw std::invalid_argument("Invalid scene number");
    return static_cast<float>(x);
}
int integer(const V& v, int lo, int hi) {
    const auto x = get<double>(v);
    if (!std::isfinite(x) || x < lo || x > hi || std::floor(x) != x)
        throw std::invalid_argument("Invalid scene enumeration or index");
    return static_cast<int>(x);
}
glm::vec3 triple(const V& v) {
    auto& a = get<A>(v);
    if (a.size() != 3)
        throw std::invalid_argument("Expected three coordinates");
    return {scalar(a[0]), scalar(a[1]), scalar(a[2])};
}
void bound_depth(const std::string& text) {
    int depth = 0;
    bool string = false, escaped = false;
    for (char c : text) {
        if (string) {
            if (escaped)
                escaped = false;
            else if (c == '\\')
                escaped = true;
            else if (c == '"')
                string = false;
        } else if (c == '"')
            string = true;
        else if (c == '{' || c == '[') {
            if (++depth > 32)
                throw std::invalid_argument("Scene nesting limit exceeded");
        } else if (c == '}' || c == ']') {
            if (--depth < 0)
                throw std::invalid_argument("Malformed JSON");
        }
    }
}
} // namespace
SceneDocument read_scene_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Cannot open scene file");
    auto length = file.tellg();
    if (length < 0 || length > static_cast<std::streamoff>(max_bytes))
        throw std::invalid_argument("Scene file exceeds 2 MiB");
    std::string text(static_cast<std::size_t>(length), ' ');
    file.seekg(0);
    if (!file.read(text.data(), length))
        throw std::runtime_error("Scene read failed");
    bound_depth(text);
    V root;
    std::string error;
    auto end = picojson::parse(root, text.cbegin(), text.cend(), &error);
    if (!error.empty() || std::any_of(end, text.cend(), [](char c) {
            return !std::isspace(static_cast<unsigned char>(c));
        }))
        throw std::invalid_argument("Malformed scene JSON");
    get<O>(root);
    integer(root.get("schema"), 1, 1);
    integer(root.get("fixed_hz"), 120, 120);
    if (triple(root.get("gravity")) != glm::vec3(0, -9.81F, 0))
        throw std::invalid_argument("Unsupported world gravity");
    SceneDocument doc;
    auto name = get<std::string>(root.get("scene"));
    if (name == "structural-lab")
        doc.kind = SceneKind::structural_lab;
    else if (name != "sandbox")
        throw std::invalid_argument("Not an editable scene");
    auto& objects = get<A>(root.get("objects"));
    if (objects.size() > 2048)
        throw std::invalid_argument("Scene object limit exceeded");
    for (auto& entry : objects) {
        get<O>(entry);
        ObjectSpec o;
        o.primitive = static_cast<Primitive>(integer(entry.get("shape"), 0, 5));
        o.finish = static_cast<Finish>(integer(entry.get("finish"), 1, 5));
        o.pose.position = triple(entry.get("position"));
        o.pose.scale = triple(entry.get("half_size"));
        auto& q = get<A>(entry.get("rotation_wxyz"));
        if (q.size() != 4)
            throw std::invalid_argument("Expected quaternion");
        o.pose.rotation = {scalar(q[0]), scalar(q[1]), scalar(q[2]), scalar(q[3])};
        o.mass = scalar(entry.get("mass"));
        o.gravity = get<bool>(entry.get("gravity_enabled"));
        o.friction = scalar(entry.get("friction"));
        o.restitution = scalar(entry.get("restitution"));
        o.velocity = triple(entry.get("velocity"));
        o.angular = triple(entry.get("angular_velocity"));
        doc.objects.push_back(o);
    }
    doc.projectile = integer(root.get("projectile"), -1, static_cast<int>(objects.size()) - 1);
    doc.launch = triple(root.get("launch_velocity"));
    auto eye = triple(root.get("eye")), d = triple(root.get("direction"));
    if (glm::length(d) < .0001F)
        throw std::invalid_argument("Invalid camera direction");
    d = glm::normalize(d);
    doc.camera = Camera(eye, std::atan2(d.z, d.x), std::asin(d.y));
    return doc;
}
void write_scene_file(const std::filesystem::path& path, SceneKind kind,
                      const SandboxEditor& editor, const Camera& camera) {
    if (!editor.building())
        throw std::invalid_argument("Return to Build before saving");
    A objects;
    int projectile = -1;
    for (auto& item : editor.items()) {
        auto& o = item.authored;
        auto q = o.pose.rotation;
        if (item.entity == editor.projectile())
            projectile = static_cast<int>(objects.size());
        objects.emplace_back(O{
            {"shape", V(double(static_cast<int>(o.primitive)))},
            {"finish", V(double(static_cast<int>(o.finish)))},
            {"position", vec(o.pose.position)},
            {"half_size", vec(o.pose.scale)},
            {"rotation_wxyz", V(A{V(double(q.w)), V(double(q.x)), V(double(q.y)), V(double(q.z))})},
            {"mass", V(double(o.mass))},
            {"gravity_enabled", V(o.gravity)},
            {"friction", V(double(o.friction))},
            {"restitution", V(double(o.restitution))},
            {"velocity", vec(o.velocity)},
            {"angular_velocity", vec(o.angular)}});
    }
    V root(O{{"schema", V(1.)},
             {"fixed_hz", V(120.)},
             {"gravity", vec({0, -9.81F, 0})},
             {"scene", V(std::string(scene_name(kind)))},
             {"projectile", V(double(projectile))},
             {"launch_velocity", vec(editor.launch_velocity)},
             {"eye", vec(camera.position())},
             {"direction", vec(camera.direction())},
             {"objects", V(objects)}});
    std::string text = root.serialize(false) + "\n";
    if (text.size() > max_bytes)
        throw std::invalid_argument("Scene too large");
    auto tmp = path;
    tmp += ".tmp";
    auto* f = std::fopen(tmp.c_str(), "wbx");
    if (!f)
        throw std::runtime_error("Cannot create temporary scene file");
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = (std::fclose(f) == 0) && ok;
    try {
        if (!ok)
            throw std::runtime_error("Scene write failed");
        std::filesystem::rename(tmp, path);
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        throw;
    }
}
} // namespace coresim
