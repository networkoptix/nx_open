const path = require('path');
const webpack = require('webpack');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CopyWebpackPlugin = require('copy-webpack-plugin');
const ExtractTextPlugin = require('extract-text-webpack-plugin');

const webCommonPath = path.join(__dirname, './app/web_common');

const babelLoader = {
    loader: 'babel-loader',
    options: {
        cacheDirectory: true,
        presets: ['babel-preset-env']
    }
};

let isProd = false;

let thingsToIgnore = [
    'src/**/__snapshots__/**',
    'src/**/*.ts',
    'web_common/styles/**',
    'scripts/**',
    'styles/**',
    '.*',
    '*.ts',
    '*.map',
    '*.js',
    'index-template.html',
    'index.html'
];

let targetEnv = process.argv[ 3 ].split('.')[ 1 ]; // keep 3rd arg as env configuration
if (targetEnv === 'prod') {
    thingsToIgnore.push('src/**/*.scss');
    isProd = true;
}

module.exports = {
    context: path.resolve(__dirname + '/app'),
    entry: {
        app: 'scripts/entrypoint.js',
        webcommon: 'web_common/scripts/entrypoint.js',
        polyfills: './polyfills.ts',
        vendor: './vendor.ts',
        appnew: './main.ts'
    },
    plugins: [
        new webpack.DefinePlugin({
            PRODUCTION: JSON.stringify(isProd)
        }),

        //Development plugins
        new webpack.ContextReplacementPlugin(
            // The (\\|\/) piece accounts for path separators in *nix and Windows
            /*/angular(\\|\/)core(\\|\/)@angular/,*/
            // /\@angular(\\|\/)core(\\|\/)esm5/,
            /angular(\\|\/)core/,
            '/app', // location of your src
            {} // a map of your routes
        ),

        //Plugins used for making templates
        new webpack.NamedModulesPlugin(),
        new HtmlWebpackPlugin({
            template: 'index-template.html',
            filename: 'index.html',
            inject: false,
            chunks: ['commons', 'polyfills','vendor', 'app', 'webcommon', 'appnew'],
            chunksSortMode: 'manual'
        }),

        new ExtractTextPlugin('styles/main.[chunkhash].css', { allChunks:true }),

        new CopyWebpackPlugin([
            {
                from: '',
                to: '',
                ignore: thingsToIgnore
            }
        ]),

        //Plugins for npm packages
        new webpack.ProvidePlugin({
            'md5': 'md5',
            '$': 'jquery',
            'jQuery': 'jquery',
            'window.jQuery': 'jquery',
            '_': 'underscore',
            'screenfull': 'screenfull',
            'jquery-mousewheel': 'jquery-mousewheel'
        }),
        new webpack.optimize.CommonsChunkPlugin({
            name: 'commons',
            filename: 'scripts/commons.[hash].js',
            minChunks: 2
        })
    ],
    resolve: {
        extensions: ['.ts', '.js'],
        alias: {
            fonts: path.join(__dirname, 'app', 'fonts'),
            scripts: path.join(__dirname, 'app', 'scripts'),
            web_common: webCommonPath
        }
    },
    module: {
        rules: [
            {
                test   : /\.ts$/,
                exclude: /node_modules/,
                loaders: [ 'ts-loader', 'angular2-template-loader' ]
            },
            {
                test: /\.js$/,
                exclude: /node_modules/,
                use: [
                    babelLoader
                ]
            },
            {
                test: /\.html$/,
                include: /app/,
                exclude: /index-template\.html$/,
                use: ['html-loader']
            },
            {
                test: /\.(png|jpe?g|gif|svg|woff|woff2|ttf|eot|ico)$/,
                loader: 'file-loader?name=assets/[name].[hash].[ext]'
            }
        ]
    }
};
