'use strict';

module.exports = function(grunt) {

  // Project configuration.
  grunt.initConfig({
    // Metadata.
    pkg: grunt.file.readJSON('package.json'),
    app_banner: '/*! Servo Debug Console - ' +
      '<%= grunt.template.today("yyyy-mm-dd") %>\n' +
      '<%= pkg.homepage ? "* " + pkg.homepage + "\\n" : "" %>' +
      '* Copyright (c) <%= grunt.template.today("yyyy") %> <%= pkg.author.name %>;' +
      ' Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */\n',

    lib_banner: '/*! Servo Javascript Client - ' +
      '<%= grunt.template.today("yyyy-mm-dd") %>\n' +
      '<%= pkg.homepage ? "* " + pkg.homepage + "\\n" : "" %>' +
      '* Copyright (c) <%= grunt.template.today("yyyy") %> <%= pkg.author.name %>;' +
      ' Licensed <%= _.pluck(pkg.licenses, "type").join(", ") %> */\n',
    
    // Build console and browserify servo lib
    browserify : {
      options: {
        alias: {
          'servo': './lib/servo.js'
        }
      },
      app : {
        src: 'app/console.js',
        dest: 'dist/console.js'
      }
    },
    
    // Build library tasks
    concat: {
      lib: {
        src: ['lib/**.js'],
        dest: 'dist/servo.js'
      },
    },
    uglify: {
      options: {
        banner: '<%= lib_banner %>'
      },
      lib: {
        src: '<%= concat.lib.dest %>',
        dest: 'dist/servo.min.js'
      },
      app: {
        src: '<%= browserify.app.dest %>',
        dest: 'dist/console.min.js'
      }
    },

    nodeunit: {
      files: ['test/**/*_test.js']
    },
    watch: {
      lib: {
        files: 'lib/**.js',
        tasks: ['nodeunit', 'browserify']
      },
      app: {
        files: 'app/**.js',
        tasks: ['concat']
      },
      test: {
        files: 'test/**.js',
        tasks: ['nodeunit']
      },
    }
  });

  // These plugins provide necessary tasks.
  grunt.loadNpmTasks('grunt-contrib-concat');
  grunt.loadNpmTasks('grunt-contrib-uglify');
  grunt.loadNpmTasks('grunt-contrib-watch');
  grunt.loadNpmTasks('grunt-contrib-nodeunit');
  grunt.loadNpmTasks('grunt-browserify');

  // Default task.
  grunt.registerTask('default', ['nodeunit', 'concat', 'browserify']);

};
