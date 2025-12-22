/* Forward declarations for the C standard library functions we use. */
int printf(const char *format, ...);
int scanf(const char *format, ...);
int putchar(int character);
void *malloc(unsigned long size);

void printInt(int value) {
    printf("%d", value);
}

void printlnInt(int value) {
    printf("%d\n", value);
}

int getInt(void) {
    int value = 0;
    if (scanf("%d", &value) != 1) {
        return 0;
    }
    return value;
}

struct Str {
    char* data;
    unsigned int length;
};

struct String {
    char *data;
    unsigned int length, capacity;
};

void print(struct Str *str) {
    for(unsigned int i = 0; i < str->length; i++) {
        putchar(str->data[i]);
    }
}
void println(struct Str *str) {
    for(unsigned int i = 0; i < str->length; i++) {
        putchar(str->data[i]);
    }
    putchar('\n');
}

void to_string(unsigned int *value, struct String *out) {
    unsigned int len = 0;
    unsigned int temp = *value;
    while(temp) {
        len++;
        temp /= 10;
    }
    if(len == 0) len = 1; // Handle zero case
    char *str = (char *)malloc(len + 1);
    str[len] = '\0';
    temp = *value;
    for(int i = len - 1; i >= 0; i--) {
        str[i] = (temp % 10) + '0',
        temp /= 10;
    }
    out->data = str;
    out->length = len;
    out->capacity = len + 1;
}

struct Str* as_str(struct String *s) {
    struct Str *str = (struct Str *)malloc(sizeof(struct Str));
    str->data = (char *)malloc(s->length + 1);
    for(unsigned int i = 0; i < s->length; i++) {
        str->data[i] = s->data[i];
    }
    str->data[s->length] = '\0';
    str->length = s->length;
    return str;
}