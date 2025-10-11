package org.example.dto;

public class ImageRequest {
    private int width;
    private int height;
    private String format;

    public ImageRequest() {}

    // getter, setter
    public int getWidth() { return width; }
    public void setWidth(int width) { this.width = width; }
    public int getHeight() { return height; }
    public void setHeight(int height) { this.height = height; }
    public String getFormat() { return format; }
    public void setFormat(String format) { this.format = format; }
}