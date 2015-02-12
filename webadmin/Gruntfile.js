// Generated on 2014-05-29 using generator-angular 0.7.1
'use strict';

// # Globbing
// for performance reasons we're only matching one level down:
// 'test/spec/{,*/}*.js'
// use this if you want to recursively match all subfolders:
// 'test/spec/**/*.js'

module.exports = function (grunt) {

    // Load grunt tasks automatically
    require('load-grunt-tasks')(grunt);

    // Time how long tasks take. Can help when optimizing build times
    require('time-grunt')(grunt);

    grunt.loadNpmTasks('grunt-protractor-webdriver');
    grunt.loadNpmTasks('grunt-protractor-runner');

    // Define the configuration for all the tasks
    grunt.initConfig({

        // Project settings
        yeoman: {
            // configurable paths
            app: require('./bower.json').appPath || 'app',
            dist: 'dist'
        },

        // Automatically inject Bower components into the app
        wiredep: {
            target: {
                src: [
                    '<%= yeoman.app %>/login.html',
                    '<%= yeoman.app %>/index.html',
                    '<%= yeoman.app %>/index.xsl'
                ],
                ignorePath: '<%= yeoman.app %>/'
            }
        },
        // Watches files for changes and runs tasks based on the changed files
        watch: {
            js: {
                files: ['<%= yeoman.app %>/scripts/{,*/}*.js'],
                tasks: ['newer:jshint:all'],
                options: {
                    livereload: true
                }
            },
            jsTest: {
                files: ['test/spec/{,*/}*.js'],
                tasks: ['newer:jshint:test', 'karma']
            },
            compass: {
                files: ['<%= yeoman.app %>/styles/{,*/}*.{scss,sass}'],
                tasks: ['compass:server', 'autoprefixer']
            },
            gruntfile: {
                files: ['Gruntfile.js']
            },
            livereload: {
                options: {
                    livereload: '<%= connect.options.livereload %>'
                },
                files: [
                    '<%= yeoman.app %>/{,*/}*.html',
                    '.tmp/styles/{,*/}*.css',
                    '<%= yeoman.app %>/images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}'
                ]
            }
        },

        // The actual grunt server settings
        connect: {
            options: {
                port: 9000,
                // Change this to '0.0.0.0' to access the server from outside.
                hostname: '0.0.0.0',
                livereload: 35729
            },
            proxies: [
                /*{
                    context: '/ec2/',
                    host: '10.0.2.229',
                    port: 7039,
                    headers: { //admin:123
                        'Authorization': 'Basic YWRtaW46MTIz'
                    }
                 },
                 {
                     context: '/',
                     host: 'mono',
                     port: 41000,
                 }
                 */

                //'Authorization': 'Basic YWRtaW46MTIz' //admin:123
                //'Authorization': 'Basic dXNlcjoxMjM='//user:123

                //Total proxy
                //{context: '/',host: '192.168.56.101',port: 7002,headers: {'Authorization': 'Basic YWRtaW46MTIz'}},


                //Evgeniy

/*                {context: '/api/', host: '192.168.56.101', port: 9000},
                {context: '/ec2/', host: '192.168.56.101', port: 9000},
                {context: '/media/', host: '192.168.56.101', port: 9000},
                {context: '/hls/', host: '192.168.56.101',port: 9000},
                {context: '/proxy/', host: '192.168.56.101',port: 9000}*/

                //Sergey Yuldashev
/*                {context: '/api/',      host: '10.0.2.203', port: 8901, headers: {'Authorization': 'Basic YWRtaW46MTIz'}},
                {context: '/ec2/',      host: '10.0.2.203', port: 8901, headers: {'Authorization': 'Basic YWRtaW46MTIz'}},
                {context: '/media/',    host: '10.0.2.203', port: 8901, headers: {'Authorization': 'Basic YWRtaW46MTIz'}},
                {context: '/hls/',      host: '10.0.2.203', port: 8901, headers: {'Authorization': 'Basic YWRtaW46MTIz'}}
*/
                // Sasha
                {context: '/api/',      host: '10.0.2.119', port: 7001},
                {context: '/ec2/',      host: '10.0.2.119', port: 7001},
                {context: '/media/',    host: '10.0.2.119', port: 7001},
                {context: '/hls/',      host: '10.0.2.119', port: 7001},
                {context: '/proxy/',    host: '10.0.2.119', port: 7001}

                //Roman Vasilenko  port: 7003,7004,7005,2006
                /*
                {context: '/api/', host: '10.0.2.244', port: 7005},
                {context: '/ec2/', host: '10.0.2.244', port: 7005},
                {context: '/hls/', host: '10.0.2.244', port: 7005},
                {context: '/media/', host: '10.0.2.244', port: 7005},
                {context: '/proxy/', host: '10.0.2.244', port: 7005}
*/
                //Daria
                //{context: '/api/', host: '10.0.2.229', port: 7039, headers: {'Authorization': 'Basic YWRtaW46MTIz'}},
                //{context: '/ec2/', host: '10.0.2.229', port: 7039, headers: {'Authorization': 'Basic YWRtaW46MTIz'}}

                //Denis
                //{context: '/api/', host: '10.0.2.182', port: 7001, headers: {'Authorization': 'Basic YWRtaW46MTIz'}},
                //{context: '/ec2/', host: '10.0.2.182', port: 7001, headers: {'Authorization': 'Basic YWRtaW46MTIz'}}
            ],
            livereload: {
                options: {
                    middleware: function (connect, options) {
                        if (!Array.isArray(options.base)) {
                            options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [require('grunt-connect-proxy/lib/utils').proxyRequest];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(connect.static(base));
                        });

                        // Make directory browse-able.
                        var directory = options.directory || options.base[options.base.length - 1];
                        middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    open: true,
                    base: [
                        '.tmp',
                        'test',
                        '<%= yeoman.app %>'
                    ]
                }
            },
            test: {
                options: {
                    middleware: function (connect, options) {
                        if (!Array.isArray(options.base)) {
                            options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [require('grunt-connect-proxy/lib/utils').proxyRequest];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(connect.static(base));
                        });

                        // Make directory browse-able.
                        var directory = options.directory || options.base[options.base.length - 1];
                        middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    port: 9000,

                    base: [
                        '.tmp',
                        'test',
                        '<%= yeoman.app %>'
                    ]
                }
            },
            dist: {
                options: {
                    middleware: function (connect, options) {
                        if (!Array.isArray(options.base)) {
                            options.base = [options.base];
                        }

                        // Setup the proxy
                        var middlewares = [require('grunt-connect-proxy/lib/utils').proxyRequest];

                        // Serve static files.
                        options.base.forEach(function (base) {
                            middlewares.push(connect.static(base));
                        });

                        // Make directory browse-able.
                        var directory = options.directory || options.base[options.base.length - 1];
                        middlewares.push(connect.directory(directory));

                        return middlewares;
                    },
                    open: true,
                    base: '<%= yeoman.dist %>'
                }
            }
        },

        // Make sure code styles are up to par and there are no obvious mistakes
        jshint: {
            options: {
                jshintrc: '.jshintrc',
                reporter: require('jshint-stylish')
            },
            all: [
                'Gruntfile.js',
                '<%= yeoman.app %>/scripts/{,*/}*.js'
            ],
            test: {
                options: {
                    jshintrc: 'test/.jshintrc'
                },
                src: ['test/spec/{,*/}*.js']
            }
        },

        // Empties folders to start fresh
        clean: {
            dist: {
                files: [
                    {
                        dot: true,
                        src: [
                            '.tmp',
                            '<%= yeoman.dist %>/*',
                            '!<%= yeoman.dist %>/.git*'
                        ]
                    }
                ]
            },
            server: '.tmp',

            publish:{
                files:[
                    {
                        dot: true,
                        src: '<%= yeoman.dist %>/../../mediaserver/maven/bin-resources/resources/static/*'
                    }
                ],
                options: {
                    force: true
                }
            }
        },

        // Add vendor prefixed styles
        autoprefixer: {
            options: {
                browsers: ['last 1 version']
            },
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '.tmp/styles/',
                        src: '{,*/}*.css',
                        dest: '.tmp/styles/'
                    }
                ]
            }
        },

        // Automatically inject Bower components into the app
        'bower-install': {
            app: {
                html: '<%= yeoman.app %>/index.html',
                ignorePath: '<%= yeoman.app %>/'
            }
        },


        // Compiles Sass to CSS and generates necessary files if requested
        compass: {
            options: {
                sassDir: '<%= yeoman.app %>/styles',
                cssDir: '.tmp/styles',
                generatedImagesDir: '.tmp/images/generated',
                imagesDir: '<%= yeoman.app %>/images',
                javascriptsDir: '<%= yeoman.app %>/scripts',
                fontsDir: '<%= yeoman.app %>/fonts',
                importPath: '<%= yeoman.app %>/bower_components',
                httpImagesPath: '/images',
                httpGeneratedImagesPath: '/images/generated',
                httpFontsPath: '/fonts',
                relativeAssets: false,
                assetCacheBuster: false,
                raw: 'Sass::Script::Number.precision = 10\n'
            },
            dist: {
                options: {
                    generatedImagesDir: '<%= yeoman.dist %>/images/generated'
                }
            },
            server: {
                options: {
                    debugInfo: true
                }
            }
        },

        // Renames files for browser caching purposes
        rev: {
            dist: {
                files: {
                    src: [
                        '<%= yeoman.dist %>/scripts/{,*/}*.js',
                        '<%= yeoman.dist %>/styles/{,*/}*.css'//,
                        //'<%= yeoman.dist %>/images/{,*/}*.{png,jpg,jpeg,gif,webp,svg}',
                        //'<%= yeoman.dist %>/fonts/*'
                    ]
                }
            }
        },

        // Reads HTML for usemin blocks to enable smart builds that automatically
        // concat, minify and revision files. Creates configurations in memory so
        // additional tasks can operate on them
        useminPrepare: {
            html: ['<%= yeoman.app %>/index.html', '<%= yeoman.app %>/login.html', '<%= yeoman.app %>/api.xsl'],
            options: {
                dest: '<%= yeoman.dist %>'
            }
        },

        // Performs rewrites based on rev and the useminPrepare configuration
        usemin: {
            html: ['<%= yeoman.dist %>/{,*/}{*.html,*.xsl}'],
            css: ['<%= yeoman.dist %>/styles/{,*/}*.css'],
            options: {
                assetsDirs: [
                    '<%= yeoman.dist %>',
                    '<%= yeoman.dist %>/fonts',

                    '<%= yeoman.app %>',
                    '<%= yeoman.app %>/fonts'
                ]
            }
        },

        // The following *-min tasks produce minified files in the dist folder
        imagemin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>/images',
                        src: '{,*/}*.{png,jpg,jpeg,gif}',
                        dest: '<%= yeoman.dist %>/images'
                    }
                ]
            }
        },
        svgmin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>/images',
                        src: '{,*/}*.svg',
                        dest: '<%= yeoman.dist %>/images'
                    }
                ]
            }
        },
        htmlmin: {
            dist: {
                options: {
                    collapseWhitespace: true,
                    collapseBooleanAttributes: true,
                    removeCommentsFromCDATA: true,
                    removeOptionalTags: true
                },
                files: [
                    {
                        expand: true,
                        cwd: '<%= yeoman.dist %>',
                        src: ['*.html', 'views/{,*/}*.html'],
                        dest: '<%= yeoman.dist %>'
                    }
                ]
            }
        },

        // Allow the use of non-minsafe AngularJS files. Automatically makes it
        // minsafe compatible so Uglify does not destroy the ng references
        ngmin: {
            dist: {
                files: [
                    {
                        expand: true,
                        cwd: '.tmp/concat/scripts',
                        src: '*.js',
                        dest: '.tmp/concat/scripts'
                    }
                ]
            }
        },

        // Replace Google CDN references
        cdnify: {
            dist: {
                html: ['<%= yeoman.dist %>/*.html']
            }
        },

        // Copies remaining files to places other tasks can use
        copy: {
            dist: {
                files: [
                    {
                        expand: true,
                        dot: true,
                        cwd: '<%= yeoman.app %>',
                        dest: '<%= yeoman.dist %>',
                        src: [
                            '*.{ico,png,txt}',
                            '.htaccess',
                            '*.html',
                            '*.xml',
                            '*.xsl',
                            'views/{,*/}*.html',
                            'customization/*',
                            //'bower_components/**/*',
                            //'bower_components/videogular-themes-default/videogular.css',
                            'images/{,*/}*.{webp}',
                            'fonts/*'
                        ]
                    },
                    {
                        expand: true,
                        cwd: '.tmp/images',
                        dest: '<%= yeoman.dist %>/images',
                        src: ['generated/*']
                    },
                    {
                        expand: true,
                        cwd: '<%= yeoman.app %>',
                        flatten: true,
                        dest: '<%= yeoman.dist %>/fonts',
                        src: ['bower_components/sass-bootstrap/fonts/*']
                    }
                ]
            },
            styles: {
                expand: true,
                cwd: '<%= yeoman.app %>/styles',
                dest: '.tmp/styles/',
                src: '{,*/}*.css'
            },
            publish:{
                expand: true,
                nonull:true,
                cwd: '<%= yeoman.app %>/../dist',
                dest: '../mediaserver/maven/bin-resources/resources/static/',
                src: '**'
            }
        },

        // Run some tasks in parallel to speed up the build process
        concurrent: {
            server: [
                'compass:server'
            ],
            test: [
                'compass'
            ],
            dist: [
                'compass:dist',
                'imagemin',
                'svgmin'
            ]
        },

        // By default, your `index.html`'s <!-- Usemin block --> will take care of
        // minification. These next options are pre-configured if you do not wish
        // to use the Usemin blocks.
        //cssmin: {
        //   dist: {
        //     files: {
        //       '<%= yeoman.dist %>/styles/main.css': [
        //         '.tmp/styles/{,*/}*.css',
        //         '<%= yeoman.app %>/styles/{,*/}*.css'
        //       ]
        //     }
        //   }
        //},
        // uglify: {
        //   dist: {
        //     files: {
        //       '<%= yeoman.dist %>/scripts/scripts.js': [
        //         '<%= yeoman.dist %>/scripts/scripts.js'
        //       ]
        //     }
        //   }
        // },
        // concat: {
        //   dist: {}
        // },

        // Test settings
        karma: {
            unit: {
                configFile: 'karma.conf.js',
                singleRun: true
            }
        },
        protractor: {
            options: {
                configFile: 'protractor-conf.js', // Default config file
                keepAlive: true, // If false, the grunt process stops when the test fails.
                noColor: false, // If true, protractor will not use colors in its output.
                args: {
                    // Arguments passed to the command
                }
            },
            all: {   // Grunt requires at least one target to run so you can simply put 'all: {}' here too.
                options: {
                    //configFile: 'e2e.conf.js', // Target-specific config file
                    args: {} // Target-specific arguments
                }
            },
            server:{

            }
        },
        protractor_webdriver: {
            options:{

            },
            default:{

            }
        }
    });


    grunt.registerTask('serve', function (target) {
        if (target === 'dist') {
            return grunt.task.run([
                'build',
                'configureProxies:server',
                //'connect:livereload',
                'connect:dist:keepalive']);
        }

        grunt.task.run([
            'clean:server',
            'wiredep',
            'bower-install',
            'concurrent:server',
            'autoprefixer',
            'configureProxies:server',
            'connect:livereload',
            'watch'
        ]);
    });

    grunt.registerTask('server', function () {
        grunt.log.warn('The `server` task has been deprecated. Use `grunt serve` to start a server.');
        grunt.task.run(['serve']);
    });

    grunt.registerTask('test', [
        'clean:server',
        'concurrent:test',
        'configureProxies:server',
        'autoprefixer',
        'connect:test',
        'protractor_webdriver',
        'protractor:all',
        'newer:jshint'
        //'karma'
    ]);
    grunt.registerTask('code', [
        'newer:jshint'
    ]);

    grunt.registerTask('build', [
        'clean:dist',
        'bower-install',
        'useminPrepare',
        'concurrent:dist',
        'autoprefixer',
        'concat',
        'ngmin',
        'copy:dist',
        'cdnify',
        'cssmin',
        'uglify',
        'rev',
        'usemin',
        'htmlmin'
    ]);

    grunt.registerTask('default', [
        'newer:jshint',
        'test',
        'build'
    ]);


    grunt.registerTask('publish', [
        'build',
        'clean:publish',
        'copy:publish'
    ]);


    grunt.registerTask('pub', [
        'publish'
    ]);
};
