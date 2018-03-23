const merge = require('webpack-merge');
const UglifyJSPlugin = require('uglifyjs-webpack-plugin');
const common = require('./webpack.common.js');
const ExtractTextPlugin = require("extract-text-webpack-plugin");

module.exports = merge(common, {
    plugins:[
        new UglifyJSPlugin({
        })
    ],
    module: {
        rules: [
            {
                test: /\.(scss|css)$/,
                use : ExtractTextPlugin.extract({
                    use: [ {
                        loader : "css-loader",
                        options: {
                            url      : false,
                            sourceMap: true
                        }
                    },
                        {
                            loader : "sass-loader",
                            options: {
                                url      : false,
                                sourceMap: true
                            }
                        } ]
                })
            }
        ]
    }
});
