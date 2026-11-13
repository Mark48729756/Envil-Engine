#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <anvil/Application.h>
#include <anvil/Renderer.h>
#include <anvil/physics/PhysicsWorld.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <cstdio>

using namespace anvil;

class Sandbox : public Application {
public:
    Sandbox() : Application(1600, 900, "ANVIL Engine - Sandbox") {}

protected:
    void onInit() override {
        renderer_ = std::make_unique<Renderer>(window_.width(), window_.height());
        sphere_ = Mesh::sphere(64);
        floor_ = Mesh::plane(40.0f, 40.0f);
        window_.setCursorCaptured(true);
        cam_.position = {0.0f, 3.0f, 14.0f};
        cam_.pitch = -8.0f;

        items_.push_back({floor_.get(), glm::mat4(1.0f), {{0.55f, 0.55f, 0.58f}, 0.8f, 0.0f}});
        physics::BodyDesc ground; ground.position={0.0f,-0.08f,0.0f}; ground.halfExtents={20.0f,0.08f,20.0f}; ground.isStatic=true; ground.useGravity=false; physics_.createBody(ground);
        // grid: X = roughness, Y = metallic
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 7; ++x) {
                Material m;
                m.albedo = (y % 2) ? glm::vec3(0.95f, 0.64f, 0.54f) : glm::vec3(0.8f, 0.1f, 0.1f);
                m.roughness = 0.05f + 0.15f * x;
                m.metallic = y / 4.0f;
                const glm::vec3 start((x - 3) * 2.2f, 1.15f + y * 2.2f, 0.0f);
                glm::mat4 model = glm::translate(glm::mat4(1.0f), start);
                items_.push_back({sphere_.get(), model, m});
                physics::BodyDesc body; body.position=start; body.halfExtents={0.92f,0.92f,0.92f}; body.mass=1.0f; body.restitution=0.22f; body.friction=0.72f;
                physicsBodies_.push_back(physics_.createBody(body));
            }
    }

    void onUpdate(float dt) override {
        if (window_.keyDown(GLFW_KEY_ESCAPE)) window_.close();
        const glm::vec2 d = window_.mouseDelta();
        cam_.yaw += d.x * 0.1f;
        cam_.pitch = glm::clamp(cam_.pitch - d.y * 0.1f, -89.0f, 89.0f);

        float speed = (window_.keyDown(GLFW_KEY_LEFT_SHIFT) ? 20.0f : 7.0f) * dt;
        if (window_.keyDown(GLFW_KEY_W)) cam_.position += cam_.forward() * speed;
        if (window_.keyDown(GLFW_KEY_S)) cam_.position -= cam_.forward() * speed;
        if (window_.keyDown(GLFW_KEY_D)) cam_.position += cam_.right() * speed;
        if (window_.keyDown(GLFW_KEY_A)) cam_.position -= cam_.right() * speed;
        if (window_.keyDown(GLFW_KEY_E)) cam_.position.y += speed;
        if (window_.keyDown(GLFW_KEY_Q)) cam_.position.y -= speed;
        time_ += dt;
        if (std::getenv("ANVIL_AUTOEXIT") && time_ > 1.0f) window_.close();   // for CI smoke test
    }

    void onFixedUpdate(float dt) override { physics_.step(dt); }

    void onRender() override {
        for (std::size_t i=0; i<physicsBodies_.size(); ++i) {
            if (const auto* body=physics_.getBody(physicsBodies_[i]))
                items_[i+1].model=glm::translate(glm::mat4(1.0f),body->desc.position);
        }
        lights_.clear();
        for (int i = 0; i < 3; ++i) {
            float a = time_ * 0.6f + i * 2.094f;
            PointLight l;
            l.position = {std::cos(a) * 9.0f, 4.0f + i * 2.0f, std::sin(a) * 4.0f + 3.0f};
            l.color = i == 0 ? glm::vec3(1.0f, 0.4f, 0.3f) : i == 1 ? glm::vec3(0.3f, 1.0f, 0.5f) : glm::vec3(0.4f, 0.5f, 1.0f);
            l.intensity = 120.0f;
            lights_.push_back(l);
        }
        renderer_->render(cam_, items_, lights_, sun_);
        if (std::getenv("ANVIL_SHOT") && time_ > 0.8f && !shot_) {   // debug: dump frame to PPM
            shot_ = true;
            int w = window_.width(), h = window_.height();
            std::vector<unsigned char> px((size_t)w * h * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
            if (FILE* f = std::fopen(std::getenv("ANVIL_SHOT"), "wb")) {
                std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                for (int y = h - 1; y >= 0; --y) std::fwrite(&px[(size_t)y * w * 3], 1, (size_t)w * 3, f);
                std::fclose(f);
            }
        }
    }

    void onResize(int w, int h) override { renderer_->resize(w, h); }

private:
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Mesh> sphere_, floor_;
    std::vector<DrawItem> items_;
    std::vector<PointLight> lights_;
    physics::PhysicsWorld physics_;
    std::vector<std::uint32_t> physicsBodies_;
    Sun sun_;
    Camera cam_;
    float time_ = 0.0f;
    bool shot_ = false;
};

int main() {
    Sandbox app;
    app.run();
    return 0;
}
