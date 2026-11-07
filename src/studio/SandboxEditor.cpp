#include <algorithm>
#include <cmath>
#include <coresim/studio/SandboxEditor.hpp>
#include <limits>
#include <stdexcept>
namespace coresim {
namespace {
bool finite(glm::vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
ObjectSpec checked(ObjectSpec o) {
    if (o.primitive < Primitive::cube || o.primitive > Primitive::plane ||
        o.finish < Finish::steel || o.finish > Finish::plastic)
        throw std::invalid_argument("Unknown primitive or finish");
    const auto& t = o.pose;
    if (!finite(t.position) || !finite(t.scale) || !finite(o.velocity) || !finite(o.angular) ||
        glm::any(glm::greaterThan(glm::abs(t.position), glm::vec3(1000))) ||
        glm::any(glm::lessThan(t.scale, glm::vec3(.05F))) ||
        glm::any(glm::greaterThan(t.scale, glm::vec3(100))) ||
        !std::isfinite(glm::length(t.rotation)) || glm::length(t.rotation) < .0001F ||
        !std::isfinite(o.mass) || (o.mass != 0 && o.mass < .01F) || o.mass > 10000 ||
        glm::any(glm::greaterThan(glm::abs(o.velocity), glm::vec3(100))) ||
        glm::any(glm::greaterThan(glm::abs(o.angular), glm::vec3(50))) ||
        !std::isfinite(o.friction) || o.friction < 0 || o.friction > 1 ||
        !std::isfinite(o.restitution) || o.restitution < 0 || o.restitution > 1)
        throw std::invalid_argument("Invalid object dimensions, pose or physical properties");
    o.pose.rotation = glm::normalize(o.pose.rotation);
    if (o.primitive == Primitive::sphere)
        o.pose.scale = glm::vec3(t.scale.x);
    if (o.primitive == Primitive::plane)
        o.mass = 0;
    if (o.mass == 0) {
        o.velocity = {};
        o.angular = {};
    }
    return o;
}
void apply(World& w, Entity e, const ObjectSpec& o) {
    w.set_transform(e, o.pose);
    w.set_mesh(e, {o.primitive == Primitive::sphere ? MeshKind::sphere : MeshKind::cube, o.finish});
    if (o.primitive == Primitive::sphere)
        w.set_collider(e, SphereCollider(o.pose.scale.x));
    else if (o.primitive == Primitive::plane)
        w.set_collider(e, PlaneCollider(glm::vec3(0, 1, 0), o.pose.scale.y));
    else
        w.set_collider(e, BoxCollider(o.pose.scale));
    RigidBody body(o.mass, o.restitution, o.friction);
    body.gravity_enabled = o.gravity;
    body.velocity = o.velocity;
    body.angular_velocity = o.angular;
    w.set_rigid_body(e, body);
}
std::optional<float> intersection(const Transform& t, const Collider& c, const Ray& r) {
    auto p = r.origin - t.position;
    if (const auto* s = std::get_if<SphereCollider>(&c)) {
        float b = glm::dot(p, r.direction), h = b * b - glm::dot(p, p) + s->radius * s->radius;
        if (h < 0)
            return {};
        float near = -b - std::sqrt(h), far = -b + std::sqrt(h);
        if (far >= 0)
            return near >= 0 ? near : far;
    } else if (const auto* box = std::get_if<BoxCollider>(&c)) {
        auto inv = glm::conjugate(t.rotation);
        p = inv * p;
        auto d = inv * r.direction;
        float lo = -std::numeric_limits<float>::infinity(), hi = -lo;
        for (int i = 0; i < 3; ++i) {
            float h = box->half_extents[i];
            if (std::abs(d[i]) < 1e-7F) {
                if (std::abs(p[i]) > h)
                    return {};
                continue;
            }
            float a = (-h - p[i]) / d[i], b = (h - p[i]) / d[i];
            if (a > b)
                std::swap(a, b);
            lo = std::max(lo, a);
            hi = std::min(hi, b);
        }
        if (hi >= lo && hi >= 0)
            return lo >= 0 ? lo : hi;
    } else {
        auto plane = std::get<PlaneCollider>(c);
        plane.normal = t.rotation * plane.normal;
        float d = glm::dot(plane.normal, r.direction);
        if (std::abs(d) > 1e-7F) {
            float v = (plane.offset - glm::dot(plane.normal, p)) / d;
            if (v >= 0)
                return v;
        }
    }
    return {};
}
} // namespace
Ray camera_ray(const Camera& camera, glm::vec2 ndc, float aspect) {
    auto p = glm::inverse(camera.projection(aspect) * camera.view()) * glm::vec4(ndc, -1, 1);
    return {camera.position(), glm::normalize(glm::vec3(p) / p.w - camera.position())};
}
ObjectSpec primitive_spec(Primitive p, glm::vec3 pos) {
    ObjectSpec o;
    o.primitive = p;
    o.pose.position = pos;
    o.pose.scale = glm::vec3(.65F);
    if (p == Primitive::sphere)
        o.finish = Finish::rubber;
    if (p == Primitive::platform || p == Primitive::ramp) {
        o.pose.scale = {2, .2F, 1};
        o.mass = 0;
        o.finish = Finish::wood;
        if (p == Primitive::ramp)
            o.pose.rotation = glm::angleAxis(.35F, glm::vec3(0, 0, 1));
    }
    if (p == Primitive::wall) {
        o.mass = 0;
        o.pose.scale = {2, 1.5F, .2F};
        o.finish = Finish::concrete;
    }
    if (p == Primitive::plane) {
        o.mass = 0;
        o.pose.scale = {12, .15F, 12};
        o.finish = Finish::concrete;
    }
    return o;
}
SandboxEditor::SandboxEditor(World& w, bool structural) : world_(w) {
    undo_.reserve(128);
    redo_.reserve(128);
    create(primitive_spec(Primitive::plane, {0, -.15F, 0}));
    if (structural) {
        auto wall = primitive_spec(Primitive::wall, {0, 1.2F, -6});
        wall.pose.scale = {9, 1.2F, .24F};
        create(wall);
        for (int layer = 0; layer < 8; ++layer)
            for (int beam = -1; beam <= 1; ++beam) {
                auto o = primitive_spec(Primitive::platform,
                                        {0, .281F + .561F * static_cast<float>(layer), 0});
                o.mass = 1.25F;
                o.friction = .75F;
                o.restitution = .04F;
                o.pose.scale = {1.45F, .28F, .46F};
                if (layer % 2) {
                    o.pose.position.x = static_cast<float>(beam) * .96F;
                    o.pose.rotation = glm::angleAxis(glm::radians(90.F), glm::vec3(0, 1, 0));
                } else
                    o.pose.position.z = static_cast<float>(beam) * .96F;
                create(o);
            }
        auto ball = primitive_spec(Primitive::sphere, {-6, 1.4F, 0});
        ball.pose.scale = glm::vec3(.85F);
        ball.mass = 22;
        ball.gravity = false;
        ball.finish = Finish::steel;
        projectile_ = create(ball);
        undo_.clear();
        return;
    }
    create(primitive_spec(Primitive::cube, {-1, .66F, 0}));
    create(primitive_spec(Primitive::sphere, {1, 2, 0}));
    selection_ = {};
    undo_.clear();
}
void SandboxEditor::require_build() const {
    if (!building_)
        throw std::logic_error("Return to Build before editing");
}
const SandboxEditor::Item* SandboxEditor::selected() const {
    if (!world_.alive(selection_))
        return nullptr;
    auto it = std::find_if(items_.begin(), items_.end(),
                           [&](auto& item) { return item.entity == selection_; });
    return it == items_.end() ? nullptr : &*it;
}
void SandboxEditor::select(Entity id) { selection_ = world_.alive(id) ? id : Entity{}; }
std::optional<float> SandboxEditor::hit(Ray r, Entity* nearest) const {
    if (nearest)
        *nearest = {};
    if (!finite(r.origin) || !finite(r.direction) || glm::length(r.direction) < 1e-6F)
        return {};
    r.direction = glm::normalize(r.direction);
    std::optional<float> best;
    for (auto& item : items_) {
        auto* t = world_.transform(item.entity);
        auto* c = world_.collider(item.entity);
        if (!t || !c)
            continue;
        auto distance = intersection(*t, *c, r);
        if (distance && (!best || *distance < *best)) {
            best = distance;
            if (nearest)
                *nearest = item.entity;
        }
    }
    return best;
}
void SandboxEditor::pick(Ray ray) {
    Entity id;
    hit(ray, &id);
    select(id);
}
Entity SandboxEditor::create(ObjectSpec object) {
    require_build();
    auto o = checked(object);
    if (items_.size() >= 2048)
        throw std::invalid_argument("Sandbox limit is 2048 objects");
    const auto previous = selected() ? selected()->key : 0;
    const auto key = next_key_++;
    Entity id = world_.create();
    try {
        apply(world_, id, o);
        items_.push_back({id, o, o.pose, key});
    } catch (...) {
        world_.destroy(id);
        throw;
    }
    selection_ = id;
    remember({key, previous, {}, o, o.pose, false});
    return id;
}
void SandboxEditor::change(ObjectSpec object) {
    require_build();
    if (!selected())
        return;
    auto o = checked(object);
    for (auto& item : items_)
        if (item.entity == selection_) {
            apply(world_, selection_, o);
            remember(
                {item.key, item.key, item.authored, o, item.original, item.entity == projectile_});
            item.authored = o;
            return;
        }
}
void SandboxEditor::erase() {
    require_build();
    auto* item = selected();
    if (!item)
        return;
    remember(
        {item->key, item->key, item->authored, {}, item->original, item->entity == projectile_});
    if (selection_ == projectile_)
        projectile_ = {};
    world_.destroy(selection_);
    std::erase_if(items_, [&](auto& i) { return i.entity == selection_; });
    selection_ = {};
}
void SandboxEditor::duplicate() {
    require_build();
    if (auto* i = selected()) {
        auto o = i->authored;
        o.pose.position.x += 2 * o.pose.scale.x + .1F;
        create(o);
    }
}
void SandboxEditor::reset_transform() {
    require_build();
    if (auto* i = selected()) {
        auto o = i->authored;
        o.pose = i->original;
        change(o);
    }
}
void SandboxEditor::remember(Command c) {
    redo_.clear();
    if (gesture_ && gesture_changed_ && c.before && c.after && !undo_.empty() &&
        undo_.back().key == c.key && undo_.back().before && undo_.back().after) {
        undo_.back().after = c.after;
        return;
    }
    if (undo_.size() == 128)
        undo_.erase(undo_.begin());
    undo_.push_back(c);
    gesture_changed_ = gesture_ && c.before && c.after;
}
void SandboxEditor::replay(const Command& c, bool forward) {
    auto target = forward ? c.after : c.before;
    auto it = std::find_if(items_.begin(), items_.end(), [&](auto& i) { return i.key == c.key; });
    if (target) {
        if (it == items_.end()) {
            Entity e = world_.create();
            try {
                apply(world_, e, *target);
                items_.push_back({e, *target, c.original, c.key});
            } catch (...) {
                world_.destroy(e);
                throw;
            }
            it = items_.end() - 1;
        } else {
            apply(world_, it->entity, *target);
            it->authored = *target;
        }
        if (c.projectile)
            projectile_ = it->entity;
    } else if (it != items_.end()) {
        if (it->entity == projectile_)
            projectile_ = {};
        world_.destroy(it->entity);
        items_.erase(it);
    }
    const auto key = forward ? (target ? c.key : 0) : c.previous_selection;
    selection_ = {};
    for (auto& i : items_)
        if (i.key == key) {
            selection_ = i.entity;
            break;
        }
}
void SandboxEditor::undo() {
    if (!can_undo())
        return;
    end_gesture();
    replay(undo_.back(), false);
    redo_.push_back(undo_.back());
    undo_.pop_back();
}
void SandboxEditor::redo() {
    if (!can_redo())
        return;
    end_gesture();
    replay(redo_.back(), true);
    undo_.push_back(redo_.back());
    redo_.pop_back();
}
void SandboxEditor::replace_objects(const std::vector<ObjectSpec>& specs, int projectile,
                                    glm::vec3 velocity) {
    if (specs.size() > 2048 || projectile < -1 || projectile >= static_cast<int>(specs.size()) ||
        !finite(velocity) || glm::length(velocity) > 100)
        throw std::invalid_argument("Invalid scene size, projectile or launch velocity");
    if (projectile >= 0 &&
        specs[static_cast<std::size_t>(projectile)].primitive != Primitive::sphere)
        throw std::invalid_argument("Projectile must be a sphere");
    for (auto& i : items_)
        world_.destroy(i.entity);
    items_.clear();
    projectile_ = selection_ = {};
    for (std::size_t i = 0; i < specs.size(); ++i) {
        auto id = create(specs[i]);
        if (static_cast<int>(i) == projectile)
            projectile_ = id;
    }
    launch_velocity = velocity;
    launched_ = false;
    selection_ = projectile_;
    undo_.clear();
    redo_.clear();
    end_gesture();
}
void SandboxEditor::launch() {
    if (building_ || !launch_ready())
        throw std::logic_error("Projectile cannot launch");
    if (!finite(launch_velocity) || glm::length(launch_velocity) > 100)
        throw std::invalid_argument("Launch speed must be at most 100 m/s");
    auto* body = world_.rigid_body(projectile_);
    body->wake();
    body->velocity = launch_velocity;
    body->gravity_enabled = true;
    launched_ = true;
}
void SandboxEditor::impulse(glm::vec3 impulse) {
    if (building_ || !selected())
        return;
    auto* body = world_.rigid_body(selection_);
    if (body->inverse_mass() == 0)
        return;
    const auto v = body->velocity + impulse * body->inverse_mass();
    if (!finite(v))
        throw std::invalid_argument("Invalid impulse");
    body->wake();
    body->velocity = v;
}
void SandboxEditor::restore() {
    launched_ = false;
    for (auto& i : items_)
        apply(world_, i.entity, i.authored);
    building_ = true;
}
} // namespace coresim
