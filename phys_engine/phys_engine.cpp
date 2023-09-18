/// по управлению: a/d влево вправо (важно вкл анлг язык на клавиатуре), r - возврат, пробел - прыжок (нельхзя делать 2 раза подряд в воздухе). Если застрянет в текстуре - не повезло, словили баг (я пытаюсь их отлавливать, но не всегда выходит, + не доделанная проблема это соударение с отрезком сбоку)

/// если окну плохо - меняйте значения width и height (по умолчанию они 120 и 30)

#include <SFML/Graphics.hpp> 
#include <fstream>
#include <stdio.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <string>
using namespace sf;

int width = VideoMode::getDesktopMode().width;
int height = VideoMode::getDesktopMode().height;
double aspect = (double)width / height;


class vec {
public: 
    double x, y;                // это и вектор, и точка, по сути. Просто понаписано всяких прикольных штук для удобства

    vec(double _x, double _y) : x(_x), y(_y) {};

    vec operator+(vec vect) { return vec(x + vect.x, y + vect.y); }
    vec operator-(vec vect) { return vec(x - vect.x, y - vect.y); }
    double operator*(vec vect) { return x * vect.x + y * vect.y; }

    vec operator*(double k) { return vec(x * k, y * k); }
    vec operator/(double k) { return vec(x / k, y / k); }


    double length() {
        return sqrt(x * x + y * y);
    }

    vec cord() {        //преобразование из моих координат в сфмл
        return { (x + 1. * aspect) / aspect / 2. * width, (-1 * y + 1.) / 2. * height};
    }
    vec anticord() {  //обратное 
        return { (x) * aspect * 2. / width - 1. * aspect, (-1 * y ) * 2. / height + 1.};
    }
};

            
vec pos(0, 0);      // центр фигуры ( ну или любая другая точка от которой удобно задавать фигуру)
vec cam(0, 0);
double m = 1, g = 5, t = 0., kv = 0.005, ku = 0.999;  // параметры системы; масса, ускорение свободного падения, время скажем так одного кадра (при расчете мгновенной скорости и перемещения), коэффиценты трения об воздух и потери энергии при ударе о поверхность
vec p(0, 0.000001); //импульс
double tochnost = 2. / height; // для текстур и коллизий с ними
double radius = height / 20;
bool jstriked = false; // прыжок доступен только после удара о поверхность, чтобы на пробел не улетать

class line {
public:
    vec d1, d2;             // наш отрезочек тире текстура (ну а что, в три д у нас полигоны - часть плоскости, значит тут будет часть прямой - отрезок.

    double s(vec dot) {         // расстояние от точки до отрезка
        if ((dot - d1) * (d2 - d1) < 0)
            return (dot - d1).length();
        if ((dot - d2) * (d1 - d2) < 0)
            return (dot - d2).length();

        double a = d1.y - d2.y, b = d2.x - d1.x, c = d1.x * d2.y - d2.x * d1.y;
        return abs((a * dot.x + b * dot.y + c)) / sqrt(a * a + b * b);
    }


    bool strike() {  //проверка соударения фигуры и текстуры (точность определения соударения так сказать)
        return s(pos) < radius / height * 2.;
    }

    vec norm() {   // единичная нормаль к поверхности, нужна будет при отражении. Я умный и сделяль все красиво, поэтому направление нормали не имеет значения
        double a = d1.y - d2.y, b = d2.x - d1.x;
        vec no(a, b);
        return no /  no.length();
    }

    bool sphere1() {                    // для соударения с крами отрезка
        return (pos - d1) * (d2 - d1) < 0;
    }
    bool sphere2() {
        return (pos - d2) * (d1 - d2) < 0;
    }
};

//line textures[]{ {{-1, -0.8}, {1, -0.8}} };             // массив текстур - отрезков, каждый задается двумя точками как в примере
line textures[]{ {{-100, -0.8}, {100, -0.8}}, {{-1, 0.5}, {0, -0.5}}, {{0, -0.5}, {1, 0.5}} };

ContextSettings settings(0, 0, 8, 1, 1, 0, false);
RenderWindow win(VideoMode(width, height), "game", Style::Fullscreen, settings);
Text text;

class Game {
public:
    void drawing() {
        // отрисовываем
        win.clear(Color(0, 0, 0, 0));
        cam.x = pos.x + 0.3 - p.x / 100;
        cam.y = pos.y - std::max(p.y / 100, -0.05);
        CircleShape circle(radius);
        circle.setFillColor(Color(255, 255, 255));
        circle.move((pos - cam).cord().x - radius, (pos - cam).cord().y - radius);
        win.draw(circle);
        for (int _i = 0; _i < sizeof(textures) / sizeof(textures[0]); _i++) {
            Vertex line[] =
            {
             Vertex(Vector2f((textures[_i].d1 - cam).cord().x, (textures[_i].d1 - cam).cord().y)),
             Vertex(Vector2f((textures[_i].d2 - cam).cord().x, (textures[_i].d2 - cam).cord().y))
            };
            win.draw(line, 2, Lines);
        }
        win.draw(text);
        win.display();
    }

    void colision() {
        for (int _i = 0; _i < sizeof(textures) / sizeof(textures[0]); _i++) {
            // удар о текстурку + изменение импульса ( угол падения равен углу отражения + потеря скорости при ударе)
            vec norm = textures[_i].norm();
            if (textures[_i].sphere1()) norm = pos - textures[_i].d1;   // удар о край коллайдера текстуры обработаем как удар об сферу
            if (textures[_i].sphere2()) norm = pos - textures[_i].d2;
            if (/*filter[_i] &&*/textures[_i].strike() && (norm * (p * norm) * (-1)) * p < 0) {
                //вот тут самое сложное в коллизии: по идее, если мы летим К тексутре, то отражаемся, а если ОТ - то пролетаем спокойно (чтобы мы не застревали внутри текстуры. На практике все работает странно, я еще в процессе отлавливания багов)
                p = (p - norm * (p * norm) / norm.length() / norm.length() * 1.75) * ku;
                jstriked = true;
            }
        }
    }

    void shooting() {
        vec mouse1 = { double(Mouse::getPosition(win).x), double( Mouse::getPosition(win).y) };
        vec mouse2(0, 0);
        vec shoot(0, 0);
        double rad = 0.;
        while (Mouse::isButtonPressed(Mouse::Left)) {
            drawing();
            mouse2 = { double(Mouse::getPosition(win).x), double(Mouse::getPosition(win).y) };
            rad = sqrt((mouse2.x - mouse1.x) * (mouse2.x - mouse1.x) + (mouse2.y - mouse1.y) * (mouse2.y - mouse1.y)) / 5.;
            if (rad > height / 20) rad = height / 20;
            CircleShape circle(rad);
            circle.setFillColor(Color::Transparent);
            circle.setOutlineThickness(rad / 6);
            circle.setOutlineColor(Color(127, 255, 212));
            circle.move(mouse2.x - rad, mouse2.y - rad);
            win.draw(circle);
            win.display();
        }
        shoot = mouse1.anticord() - mouse2.anticord();
        shoot = shoot / shoot.length();
        p = p + shoot * 4 * m;

    }

};


//std::vector<bool> filtered(line array[])          фильтр для оптимизации когда много тексутр, пока отложил, написан бред, а как написать нормально - идей пока нет
//{
//    std::vector<bool> ans;
//    for (int i = 0; i < sizeof(array) / sizeof(array[0]); i++)
//    {
//        vec b1((- 1.1 * aspect * pixelAspect) + pos.x + 0.3 - p.x / 100, -1.1), b2((1.1 * aspect * pixelAspect) + pos.x + 0.3 - p.x / 100, 1.1);
//        if ((array[i].d1 > b1 && array[i].d1 < b2) || (array[i].d2 > b1 && array[i].d2 < b2))
//            ans.push_back(true);
//        else ans.push_back(false);
//    }
//    return ans;
//}

int main() {
    
    Game game;

    char key = '+';
    bool shoot = false;
    std::string fps = "0";
    /*std::vector<bool> filter;*/

    Font font;
    if (!font.loadFromFile("caviar-dreams.ttf"))
    {
        win.clear(Color(255, 69, 0, 0));
        win.display();
        return 0;
    }

    while(win.isOpen()) {
        auto start = std::chrono::system_clock::now();
        /*filter = filtered(textures);*/

        game.colision();
        game.drawing();

        auto pause1 = std::chrono::system_clock::now();
        Vector2i mouse = Mouse::getPosition(win);
        if (Mouse::isButtonPressed(Mouse::Left) && (mouse.x - (pos - cam).cord().x) * (mouse.x - (pos - cam).cord().x) + (mouse.y - (pos - cam).cord().y) * (mouse.y - (pos - cam).cord().y) <= radius * radius)
        {
            game.shooting();
        }
        auto pause2 = std::chrono::system_clock::now();


        Event event;
        while (win.pollEvent(event))
        {
            if (event.type == Event::Closed)
                win.close();

            if (event.type == Event::KeyPressed)
            {

                switch (event.key.code) {
                case Keyboard::Escape:
                    win.close();
                    break;
                case Keyboard::Space:
                    if (jstriked) {
                        p.y += 2;
                        jstriked = false;
                    }
                    break;
                case Keyboard::A:
                    p.x -= 0.4 * std::max(2 + p.x, 0.);
                    break;
                case Keyboard::D:
                    p.x += 0.4 * std::max(2 - p.x, 0.);
                    break;
                case Keyboard::R:
                    p = { 0, 0 }; pos = { 0, 0 };
                    break;
                }
            }     
        }

        //само передвижение: (по сути я вычисляю мгновенную скорость и смещение за время кадра, которое принимаем малым)
        pos = pos + (p / m) * t;

        p.y -= m * g * t;  // сила тяжести
        p = p - (p / m) / (p / m).length() * ((p / m) * (p / m)) * kv * t; // трение воздуха

       
        auto end = std::chrono::system_clock::now();
        t = std::chrono::duration_cast<std::chrono::milliseconds>(end - pause2 + pause1 - start).count() / 1000.;   // время кадра

        fps = std::to_string(int(1 / t));

        text.setFont(font);
        text.setString("fps: " + fps + "\npos: " + std::to_string(pos.x) + " " + std::to_string(pos.y) + "\np: " + std::to_string(p.x) + " " + std::to_string(p.y));

        text.setCharacterSize(20);
    }
}