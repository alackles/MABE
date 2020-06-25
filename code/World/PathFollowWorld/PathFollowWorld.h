//  MABE is a product of The Hintze Lab @ MSU
//     for general research information:
//         hintzelab.msu.edu
//     for MABE documentation:
//         github.com/Hintzelab/MABE/wiki
//
//  Copyright (c) 2015 Michigan State University. All rights reserved.
//     to view the full license, visit:
//         github.com/Hintzelab/MABE/wiki/License

#pragma once					// directive to insure that this .h file is only included one time

#include <World/AbstractWorld.h> // AbstractWorld defines all the basic function templates for worlds
#include <string>
#include <memory> // shared_ptr
#include <map>

#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>

using std::shared_ptr;
using std::string;
using std::map;
using std::unordered_map;
using std::unordered_set;
using std::to_string;

class PathFollowWorld : public AbstractWorld {

public:
	// parameters for group and brain namespaces
    static shared_ptr<ParameterLink<int>> evaluationsPerGenerationPL;
    static shared_ptr<ParameterLink<std::string>> mapNamesPL;
    static shared_ptr<ParameterLink<int>> randomizeTurnSignsPL;
    static shared_ptr<ParameterLink<int>> stepsPL;
    static shared_ptr<ParameterLink<bool>> binaryInputsPL;
    static shared_ptr<ParameterLink<double>> emptySpaceCostPL;

    // a local variable used for faster access to the ParameterLink value
    int evaluationsPerGeneration;
    std::vector<string> mapNames;
    int randomizeTurnSigns;
    int steps;
    bool binaryInputs;
    double emptySpaceCost;

    std::string groupName = "root::";
    std::string brainName = "root::";
    
    // point2d defines a 2d vector with addtion, subtraction, dot/scalar product(*)
    // and cross product
    // also included are distance functions and functions which return the signed
    // angle between 2 point2ds (relitive to 0,0)

    class Point2d { // a point class, useful for 2d worlds
    public:
        double x;
        double y;

        const double pi = atan(1.0) * 4;

        Point2d() {
            x = 0.0;
            y = 0.0;
        }
        Point2d(double _x, double _y) : x(_x), y(_y) {} // construct with x,y

        void set(double _x, double _y) {
            x = _x;
            y = _y;
        }

        void show() { // print x and y of this
            std::cout << x << "," << y << "\n";
        }

        Point2d operator=(Point2d other) { // scalar/dot product of this and other
            this->x = other.x;
            this->y = other.y;
            return *this;
        }

        double operator*(Point2d other) { // scalar/dot product of this and other
            return x * other.x + y * other.y;
        }

        Point2d scale(double mag) { // vector * scalar
            Point2d newVect(x * mag, y * mag);
            return newVect;
        }

        double cross_prod(Point2d other) // cross product of this and other
        {
            return x * other.y - y * other.x;
        }

        Point2d operator+(Point2d other) { // add this point and other
            Point2d newVect(x + other.x, y + other.y);
            return newVect;
        }

        Point2d operator-(Point2d other) { // subtract other from this point
            Point2d newVect;
            newVect.x = x - other.x;
            newVect.y = y - other.y;
            return newVect;
        }

        bool operator==(Point2d other) { // scalar/dot product of this and other
            if (x == other.x && y == other.y) {
                return true;
            }
            return false;
        }

        double dist() { // length between this point and 0,0
            return sqrt(x * x + y * y);
        }

        double dist(Point2d other) { // length between this point and other
            return (*this - other).dist();
        }

        double angle_between_radian(Point2d other) // return angle in radians between
                                                   // this point and other relative to
                                                   // origin (0,0)
        {
            if (abs(x - other.x) < .000001 &&
                abs(y - other.y) < .000001) { // vectors are effecvily the same
                return (0);
            }
            if (abs(x / other.x - y / other.y) <
                .000001) { // vectors are effecvily parallel
                if (((x > 0) == (other.x > 0)) &&
                    ((y > 0) == (other.y > 0))) { // and are pointing the same way
                    return (0);
                }
            }
            return (cross_prod(other) < 0 ? 1 : -1) *
                acos((*this) * other / (dist() * other.dist()));
        }

        double angle_between_deg(Point2d other) // return angle in degrees between
                                                // this point and other relative to
                                                // origin (0,0)
        {
            return angle_between_radian(other) / pi * 180;
        }
    };

    template <typename T> class Vector2d {
        std::vector<T> data;
        int R, C;

        // get index into data vector for a given x,y
        inline int getIndex(int r, int c) { return (r * C) + c; }

    public:
        Vector2d() {
            R = 0;
            C = 0;
        }
        // construct a vector of size x * y
        Vector2d(int x, int y) : R(y), C(x) { data.resize(R * C); }

        Vector2d(int x, int y, T value) : R(y), C(x) { data.resize(R * C, value); }

        void reset(int x, int y) {
            R = y;
            C = x;
            data.clear();
            data.resize(R * C);
        }

        void reset(int x, int y, T value) {
            R = y;
            C = x;
            data.clear();
            data.resize(R * C, value);
        }

        // overwrite this classes data (vector<T>) with data coppied from newData
        void assign(std::vector<T> newData) {
            if ((int)newData.size() != R * C) {
                std::cout << "  ERROR :: in Vector2d::assign() vector provided does not "
                    "fit. provided vector is size "
                    << newData.size() << " but Rows(" << R << ") * Columns(" << C
                    << ") == " << R * C << ". Exitting." << std::endl;
                exit(1);
            }
            data = newData;
        }

        // provides access to value x,y can be l-value or r-value (i.e. used for
        // lookup of assignment)
        T& operator()(int x, int y) { return data[getIndex(y, x)]; }

        T& operator()(double x, double y) {
            return data[getIndex((int)(y), (int)(x))];
        }

        T& operator()(std::pair<int, int> loc) {
            return data[getIndex(loc.second, loc.first)];
        }

        T& operator()(std::pair<double, double> loc) {
            return data[getIndex((int)(loc.second), (int)(loc.first))];
        }

        T& operator()(Point2d loc) { return data[getIndex((int)loc.y, (int)loc.x)]; }

        // show the contents of this Vector2d with index values, and x,y values
        void show() {
            for (int r = 0; r < R; r++) {
                for (int c = 0; c < C; c++) {
                    std::cout << getIndex(r, c) << " : " << c << "," << r << " : "
                        << data[getIndex(r, c)] << "\n";
                }
            }
        }

        // show the contents of this Vector2d in a grid
        void showGrid(int precision = -1) {
            if (precision < 0) {
                for (int r = 0; r < R; r++) {
                    for (int c = 0; c < C; c++) {
                        std::cout << data[getIndex(r, c)] << " ";
                    }
                    std::cout << "\n";
                }
            }
            else {
                for (int r = 0; r < R; r++) {
                    for (int c = 0; c < C; c++) {
                        if (data[getIndex(r, c)] == 0) {
                            std::cout << std::setfill(' ') << std::setw((precision * 2) + 2)
                                << " ";
                        }
                        else {
                            std::cout << std::setfill(' ') << std::setw((precision * 2) + 1)
                                << std::fixed << std::setprecision(precision)
                                << data[getIndex(r, c)] << " ";
                        }
                    }
                    std::cout << "\n";
                }
            }
        }
        int x() { return C; }

        int y() { return R; }
    };

    std::array<int, 8> dx = {  0, 1, 1, 1, 0,-1,-1,-1 };
    std::array<int, 8> dy = { -1,-1, 0, 1, 1, 1, 0,-1 };

    std::vector<Vector2d<int>> maps;
    std::vector<std::pair<int, int>> mapSizes;
    std::vector<std::pair<int, int>> startLocations;
    std::vector<double> maxScores;
    std::vector<int> forwardCounts;

    void loadMaps(std::vector<string>& mapNames, std::vector<Vector2d<int>>& maps, std::vector<std::pair<int, int>>& mapSizes, std::vector<std::pair<int, int>>& startLocations);

    PathFollowWorld(shared_ptr<ParametersTable> PT_);
	virtual ~PathFollowWorld() = default;

	virtual auto evaluate(map<string, shared_ptr<Group>>& /*groups*/, int /*analyze*/, int /*visualize*/, int /*debug*/) -> void override;

	virtual auto requiredGroups() -> unordered_map<string,unordered_set<string>> override;
};

