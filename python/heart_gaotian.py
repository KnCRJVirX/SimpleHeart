from tkinter import *
from math import sin, cos, pi, log
import random


CANVAS_WIDTH = 640
CANVAS_HEIGHT = 480
CANVAS_CENTER_X = CANVAS_WIDTH / 2
CANVAS_CENTER_Y = CANVAS_HEIGHT / 2
IMAGE_ENLARGE = 8
HEART_COLOR = "#ff7171"
SHINY_COLOR = "#ffd6dd"


def heart_function(t, shrink_ratio=IMAGE_ENLARGE):
    # base function
    x = 16 * (sin(t) ** 3)
    y = -(13 * cos(t) - 5 * cos(2 * t) - 2 * cos(3 * t) - cos(4 * t))

    # enlarge
    x *= shrink_ratio
    y *= shrink_ratio

    # shift
    x += CANVAS_CENTER_X
    y += CANVAS_CENTER_Y

    return int(x), int(y)


def scatter_inside(x, y, beta=0.15):
    ratiox = - beta * log(random.random())
    ratioy = - beta * log(random.random())

    dx = ratiox * (x - CANVAS_CENTER_X)
    dy = ratioy * (y - CANVAS_CENTER_Y)

    return x - dx, y - dy


def shrink(x, y, ratio):
    force = -1 / (((x - CANVAS_CENTER_X) ** 2 + (y - CANVAS_CENTER_Y) ** 2) ** 0.6)
    dx = ratio * force * (x - CANVAS_CENTER_X)
    dy = ratio * force * (y - CANVAS_CENTER_Y)

    return x - dx, y - dy


class Heart:
    def __init__(self):
        self._points = set()
        self._extra_points = set()
        self._inside = set()
        self.all_points = {}
        self.random_halo = 1000
        self.build(2000)

    def build(self, number):
        # Heart
        for _ in range(number):
            t = random.uniform(0, 2 * pi)
            x, y = heart_function(t)
            self._points.add((int(x), int(y)))

        # Heart immediate inside
        for xx, yy in list(self._points):
            for _ in range(3):
                x, y = scatter_inside(xx, yy, 0.05)
                self._extra_points.add((x, y))

        # Inside
        point_list = list(self._points)
        for _ in range(4000):
            x, y = random.choice(point_list)
            x, y = scatter_inside(x, y)
            self._inside.add((int(x), int(y)))

    def calc_position(self, x, y, ratio):
        force = 1 / (((x - CANVAS_CENTER_X) ** 2 + (y - CANVAS_CENTER_Y) ** 2) ** 0.6)
        dx = ratio * force * (x - CANVAS_CENTER_X) + random.randint(-1, 1)
        dy = ratio * force * (y - CANVAS_CENTER_Y) + random.randint(-1, 1)

        return x - dx, y - dy

    def calc(self, frame):
        calc_position = self.calc_position
        wave = sin(frame / 10 * pi)
        halo_wave = wave * wave
        ratio = 10 * wave
        random_halo = self.random_halo
        halo_radius = int(8 + 36 * halo_wave)
        halo_number = int(3000 + 4000 * halo_wave)
        all_points = []

        # halo
        heart_halo = set()
        for _ in range(halo_number + random_halo):
            t = random.uniform(0, 2 * pi)
            x, y = heart_function(t, shrink_ratio=8)
            x, y = shrink(x, y, halo_radius)
            if (x, y) not in heart_halo:
                heart_halo.add((x, y))
                x += random.randint(-14, 14)
                y += random.randint(-14, 14)
                size = random.choice((1, 1, 2))
                all_points.append((x, y, size, HEART_COLOR))

        # outline
        for x, y in self._points:
            x, y = calc_position(x, y, ratio)
            size = random.randint(1, 3)
            all_points.append((x, y, size, HEART_COLOR))

        # inner
        for x, y in self._extra_points:
            x, y = calc_position(x, y, ratio)
            size = random.randint(1, 2)
            all_points.append((x, y, size, HEART_COLOR))

        for x, y in self._inside:
            x, y = calc_position(x, y, ratio)
            size = random.randint(1, 2)
            color = random.choice((HEART_COLOR, HEART_COLOR, SHINY_COLOR))
            all_points.append((x, y, size, color))

        self.all_points[frame] = all_points

    def render(self, canvas, frame):
        for x, y, size, color in self.all_points[frame % 20]:
            canvas.create_rectangle(x, y, x+size, y+size, width=0, fill=color)


def draw(root: Tk, canvas: Canvas, heart: Heart, frame=0):
    canvas.delete('all')
    heart.render(canvas, frame)
    root.after(30, draw, root, canvas, heart, frame+1)


if __name__ == "__main__":
    root = Tk()
    canvas = Canvas(root, bg='black', height=CANVAS_HEIGHT, width=CANVAS_WIDTH)
    canvas.pack()
    heart = Heart()
    for frame in range(20):
        heart.calc(frame)
    draw(root, canvas, heart)
    root.mainloop()
