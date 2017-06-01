
const int WIDTH = 480;
const int HEIGHT = 320;
uint16_t *fb;

int init_fb() {
	if (fb != NULL) {
		fb = malloc(sizeof(uint16_t) * WIDTH * HEIGHT);
		if (fb == NULL) {
			return 0;
		} else return 1;
	} else return 1;
}

void draw_icon(uint16_t *img, int x0, int y0) {
	for (int y = y0; y < (y0 + 16); y++) {
		for (int x = x0; x < (x0 + 16); x++) {
			draw_pixel_val(*(img + (y * 16) + x), x, y);
		}
	}
}

void print_str(uint16_t r, uint16_t g, uint16_t b, int x, int y, char *str) {
	int len = strlen(str);

	for (int i = 0; i < len; i++) {
		
	}
}

void draw_char(uint16_t r, uint16_t g, uint16_t b, int x, int y, char c) {

}

void draw_fill_circle(uint16_t r, uint16_t g, uint16_t b, int x, int y, int r) {
	int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
		draw_straigt_line(r, g, b, x0 + x, y0 + y, x0 - x, y0 + y);
		draw_straight_line(r, g, b, x0 + y, y0 + x, x0 - y, y0 + x);
		draw_straight_line(r, g, b, x0 - x, y0 - y, x0 + x, y0 - y);
		draw_straight_line(r, g, b, x0 - y, y0 - x, x0 + y, y0 - x);

        y += 1;
        if (err <= 0)
        {
            err += 2*y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void draw_circle(uint16_t r, uint16_t g, uint16_t b, int x0, int y0, int r)
{
	int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y)
    {
        draw_pixel(r, g, b, x0 + x, y0 + y);
        draw_pixel(r, g, b, x0 + y, y0 + x);
        draw_pixel(r, g, b, x0 - y, y0 + x);
        draw_pixel(r, g, b, x0 - x, y0 + y);
        draw_pixel(r, g, b, x0 - x, y0 - y);
        draw_pixel(r, g, b, x0 - y, y0 - x);
        draw_pixel(r, g, b, x0 + y, y0 - x);
        draw_pixel(r, g, b, x0 + x, y0 - y);

        y += 1;
        if (err <= 0)
        {
            err += 2*y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

void draw_fill_rect(uint16_t r, uint16_t g, uint16_t b, int x0, int y0, int x1, int y1) 
{
	if (x0 > x1) {
		int temp = x1;
		x1 = x0;
		x0 = temp;
	}
	if (y0 > y1) {
		int temp = y1;
		y1 = y0;
		y0 = temp;
	}

	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			draw_pixel(r, g, b, y, x);
		}
	}
}

void draw_rect(uint16_t r, uint16_t g, uint16_t b, int x, int y, int x1, int y1) {
	draw_straight_line(r,g,b,x,y,x1,y);
	draw_straight_line(r,g,b,x,y,x,y1);
	draw_straight_line(r,g,b,x1,y,x1,y1);
	draw_straight_line(r,g,b,x,y1,x1,y1);
}

void draw_straight_line(uint16_t r, uint16_t g, uint16_t b, int x, int y , int x1, int y1) {
	if (x == x1) {
		if (y < y1) {
			int temp = y1;
			y1 = y;
			y = temp;
		}
		for (int i = y; y >= y1; y--) {
			draw_pixel(r,g,b,x,i);
		}
	} else {
		if (x < x1) {
			int temp = x1;
			x1 = x;
			x = temp;
		}
		for (int i = x; x >= x1; x--) {
			draw_pixel(r,g,b,i,y);
		}
	}
}

void draw_pixel(uint16_t r, uint16_t g, uint16_t b, int x, int y) {
	unit16_t point = 0;
	point |= r;
	point = point << 6;
	point |= g;
	point = point << 5;
	point |= b;
	draw_pixel_val(point, x, y);
}

void draw_pixel_val(uint16_t color, int x, int y) {
	*(fb + (y * WIDTH) + x) = color;
}
